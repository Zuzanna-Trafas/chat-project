#include<stdio.h>
#include<sys/types.h>
#include<sys/ipc.h>
#include<sys/msg.h>
#include<stdlib.h>
#include<fcntl.h>
#include<string.h>
#include<unistd.h>
#include<signal.h>
#include<sys/sem.h>
#include<sys/shm.h>
#include"structures.h"

int userQueue;
int blockedUsersShmid;
int blockedUsersNumberShmid;
int semBlockedUsers;
int blockedGroupsShmid;
int blockedGroupsNumberShmid;
int semBlockedGroups;


struct sembuf semOpen = {0, 1, 0};
struct sembuf semClose = {0, -1, 0};


void closeQueues();
int login(int ipc);
void sendMessage();
void sendGroupMessage();
void getMessage(int ipc, int userId, blockedUser *blockedUsers, blockedGroup *blockedGroups, int *blockedUsersNumber, int *blockedGroupsNumber);
void logout(int *loggedIn);
void viewActive();
void viewGroups();
void viewGroupMembers();
void joinGroup();
void leaveGroup();
void blockMessages(blockedUser *blockedUsers, blockedGroup *blockedGroups, int *blockedUsersNumber, int *blockedGroupsNumber);

void readString(char text[48]);
void readMessage(char message[2048]);


int main() {

    int key = 1000;
    int ipc = msgget(key, 0666|IPC_CREAT);

    while(1) {
        int userId = login(ipc);

        blockedUser *blockedUsers;
        blockedGroup *blockedGroups;

        blockedUsersShmid = shmget(1200 + userId, 30*sizeof(blockedUser), 0666|IPC_CREAT);
        blockedUsers = (blockedUser*)shmat(blockedUsersShmid, 0, 0);
        semBlockedUsers = semget(1300 + userId, 1, 0666|IPC_CREAT);
        semop(semBlockedUsers, &semOpen, 1);
        int *blockedUsersNumber;
        blockedUsersNumberShmid = shmget(1400 + userId, 1, 0666|IPC_CREAT);
        blockedUsersNumber = (int*)shmat(blockedUsersNumberShmid, 0, 0);
        *blockedUsersNumber = 0;

        blockedGroupsShmid = shmget(1201 + userId, 15*sizeof(blockedGroup), 0666|IPC_CREAT);
        blockedGroups = (blockedGroup*)shmat(blockedGroupsShmid, 0, 0);
        semBlockedGroups = semget(1301 + userId, 1, 0666|IPC_CREAT);
        semop(semBlockedGroups, &semOpen, 1);
        int *blockedGroupsNumber;
        blockedGroupsNumberShmid = shmget(1401 + userId, 1, 0666|IPC_CREAT);
        blockedGroupsNumber = (int*)shmat(blockedGroupsNumberShmid, 0, 0);
        *blockedGroupsNumber = 0;

        userQueue = msgget(1000 + userId, 0666 | IPC_CREAT);
        int f = fork();
        if (f != 0) {

            int loggedIn = 1;
            while (loggedIn == 1) {
                printf("\nWhat would you like to do next?\n");
                printf("Possible actions are:\n"
                       "  1 - send message to a user,\n"
                       "  2 - send message to a group,\n"
                       "  3 - view all active users,\n"
                       "  4 - see all existing groups,\n"
                       "  5 - join a group,\n"
                       "  6 - view group members,\n"
                       "  7 - leave a group,\n"
                       "  8 - block a user or group\n"
                       "  9 - logout\n"
                       "  0 - close\n\n");

                fflush(stdin);
                char getCommand = getchar();
                if (getCommand == '\n') {
                    getCommand = getchar();
                }
                switch(getCommand) {
                    case '1':
                        sendMessage();
                        break;
                    case '2':
                        sendGroupMessage();
                        break;
                    case '3':
                        viewActive();
                        break;
                    case '4':
                        viewGroups();
                        break;
                    case '5':
                        joinGroup();
                        break;
                    case '6':
                        viewGroupMembers();
                        break;
                    case '7':
                        leaveGroup();
                        break;
                    case '8':
                        blockMessages(blockedUsers, blockedGroups, blockedUsersNumber, blockedGroupsNumber);
                        break;
                    case '9':
                        logout(&loggedIn);
                        break;
                    case '0':
                        closeQueues();
                        break;
                    default:
                        printf("Unknown command. Try again.\n");
                        break;
                }
            }
        } else {
            while (1) {
                getMessage(ipc, userId, blockedUsers, blockedGroups, blockedUsersNumber, blockedGroupsNumber);
            }
        }
    }
}

int login(int ipc) {
    int userId = 0;
    int success = 0;
    int tries = 0;
    static char userLogin[32];
    static char userPassword[32];
    memset(userLogin, 0, sizeof(userLogin));
    memset(userPassword, 0, sizeof(userPassword));

    while (success == 0) {
        printf("Login:\n");
        readString(userLogin);
        printf("Password:\n");
        readString(userPassword);

        loginInfo loginInfo1 = {2, "", ""};
        strcpy(loginInfo1.login, userLogin);
        strcpy(loginInfo1.password, userPassword);
        sendUserId sendUserId1 = {};

        msgsnd(ipc, &loginInfo1, sizeof(loginInfo) - sizeof(long), 0);
        msgrcv(ipc, &sendUserId1, sizeof(sendUserId) - sizeof(long), 3, 0);
        userId = sendUserId1.id;
        tries = sendUserId1.tries;
        if (tries < 3) {
            if (userId > 0) {
                success = 1;
            } else {
                if (tries != 0) {
                    printf("Incorrect password. Try again.\nYou have %d tries more.\n\n", 3 - tries);
                } else {
                    printf("Username not valid. Try again.\n\n");
                }
            }
        } else if (tries == 3) {
            printf("Too many tries. Account blocked.\n\n");
        } else {
            printf("Account blocked.\n\n");
        }
    }
    return userId;
}

void sendMessage() {
    command command1 = {1, 1};
    msgsnd(userQueue, &command1, sizeof(command) - sizeof(long), 0);
    sendReceiver receiver1 = {2, "", 0};
    char receiver[32];
    printf("To whom would you like to write?\n");
    readString(receiver);
    strcpy(receiver1.receiver, receiver);
    msgsnd(userQueue, &receiver1, sizeof(sendReceiver) - sizeof(long), 0);
    msgrcv(userQueue, &receiver1, sizeof(sendReceiver) - sizeof(long), 2, 0);
    if (receiver1.isValid == 0) {
        printf("Incorrect username. Try again.\n");
        return;
    } else if (receiver1.isValid == -1) {
        printf("Receiver is not active. Try again later.\n");
        return;
    }
    message newMessage = {3, "", "", "", 0};
    char messageText[2048];
    printf("Message:\n");
    readMessage(messageText);
    strcpy(newMessage.text, messageText);
    msgsnd(userQueue, &newMessage, sizeof(message) - sizeof(long), 0);
}

void sendGroupMessage() {
    command command1 = {1, 2};
    msgsnd(userQueue, &command1, sizeof(command) - sizeof(long), 0);
    sendReceiver receiver1 = {2, "", 0};
    char receiver[48];
    printf("To which group would you like to write?\n");
    readString(receiver);
    strcpy(receiver1.receiver, receiver);
    msgsnd(userQueue, &receiver1, sizeof(sendReceiver) - sizeof(long), 0);
    msgrcv(userQueue, &receiver1, sizeof(sendReceiver) - sizeof(long), 2, 0);
    if (receiver1.isValid == 0) {
        printf("Incorrect group name. Try again.\n");
        return;
    }
    message newMessage = {3, "", "", "", 0};
    strcpy(newMessage.groupName, receiver);
    char messageText[2048];
    printf("Message:\n");
    readMessage(messageText);
    strcpy(newMessage.text, messageText);
    msgsnd(userQueue, &newMessage, sizeof(message) - sizeof(long), 0);
}

void getMessage(int ipc, int userId, blockedUser *blockedUsers, blockedGroup *blockedGroups, int *blockedUsersNumber, int *blockedGroupsNumber) {
    message getMessage = {};
    msgrcv(ipc, &getMessage, sizeof(message) - sizeof(long), 1000 + userId, 0);
    if (strcmp(getMessage.sender, "") == 0) {
        return;
    }
    if (getMessage.close == 1) {
        semop(semBlockedUsers, &semClose, 1);
        for (int i = 0; i < *blockedUsersNumber; i++) {
            strcpy(blockedUsers[i].username, "");
        }
        *blockedUsersNumber = 0;
        semop(semBlockedUsers, &semOpen, 1);

        semop(semBlockedGroups, &semClose, 1);
        for (int i = 0; i < *blockedGroupsNumber; i++) {
            strcpy(blockedGroups[i].name, "");
        }
        *blockedGroupsNumber = 0;
        semop(semBlockedGroups, &semOpen, 1);

        exit(0);
    } else if (getMessage.close == -1) {
        kill(getppid(), 2);
        exit(0);
    }
    if (getMessage.groupName[0] == '\0') {
        semop(semBlockedUsers, &semClose, 1);
        for (int i = 0; i < *blockedUsersNumber; i++) {
            if(strcmp(getMessage.sender, blockedUsers[i].username) == 0) {
                semop(semBlockedUsers, &semOpen, 1);
                return;
            }
        }
        semop(semBlockedUsers, &semOpen, 1);
        printf("\n##################################################\n");
        printf("\nYou've got a new message from %s saying:\n", getMessage.sender);
        printf("%s\n", getMessage.text);
        printf("\n##################################################\n");
    } else {
        semop(semBlockedGroups, &semClose, 1);
        for (int i = 0; i < *blockedGroupsNumber; i++) {
            if(strcmp(getMessage.groupName, blockedGroups[i].name) == 0) {
                semop(semBlockedGroups, &semOpen, 1);
                return;
            }
        }
        semop(semBlockedGroups, &semOpen, 1);
        printf("\n##################################################\n");
        printf("\nYou've got a new message from %s (%s) saying:\n", getMessage.groupName, getMessage.sender);
        printf("%s\n", getMessage.text);
        printf("\n##################################################\n");
        semop(semBlockedGroups, &semOpen, 1);
    }
}

void logout(int *loggedIn) {
    command command1 = {1, 9};
    msgsnd(userQueue, &command1, sizeof(command) - sizeof(long), 0);
    printf("Bye!\n\n");
    *loggedIn = 0;
    semctl(semBlockedUsers, 1, IPC_RMID, NULL);
    semctl(semBlockedGroups, 1, IPC_RMID, NULL);
    shmctl(blockedUsersShmid, IPC_RMID, NULL);
    shmctl(blockedUsersNumberShmid, IPC_RMID, NULL);
    shmctl(blockedGroupsShmid, IPC_RMID, NULL);
    shmctl(blockedGroupsNumberShmid, IPC_RMID, NULL);
}

void viewActive() {
    command command1 = {1, 3};
    msgsnd(userQueue, &command1, sizeof(command) - sizeof(long), 0);
    activeUsersCount activeUsersCount1 = {};
    msgrcv(userQueue, &activeUsersCount1, sizeof(activeUsersCount) - sizeof(long), 2, 0);
    sendUsername sendUsername1 = {};
    for (int i = 0; i < activeUsersCount1.count; i++) {
        msgrcv(userQueue, &sendUsername1, sizeof(sendUsername) - sizeof(long), 3, 0);
        printf("%s ", sendUsername1.username);
    }
    printf("\n");
}

void viewGroups() {
    command command1 = {1, 4};
    msgsnd(userQueue, &command1, sizeof(command) - sizeof(long), 0);
    activeUsersCount activeUsersCount1 = {};
    msgrcv(userQueue, &activeUsersCount1, sizeof(activeUsersCount) - sizeof(long), 2, 0);
    sendGroupName sendGroupName1 = {};
    for (int i = 0; i < activeUsersCount1.count; i++) {
        msgrcv(userQueue, &sendGroupName1, sizeof(sendGroupName) - sizeof(long), 3, 0);
        printf("%s ", sendGroupName1.groupName);
    }
    printf("\n");
}

void viewGroupMembers() {
    command command1 = {1, 6};
    msgsnd(userQueue, &command1, sizeof(command) - sizeof(long), 0);
    printf("Which group's members would you like to see?\n");
    char groupName[48];
    readString(groupName);
    sendGroupName sendGroupName1 = {4, ""};
    strcpy(sendGroupName1.groupName, groupName);
    msgsnd(userQueue, &sendGroupName1, sizeof(sendGroupName) - sizeof(long), 0);
    activeUsersCount activeUsersCount1 = {};
    msgrcv(userQueue, &activeUsersCount1, sizeof(activeUsersCount) - sizeof(long), 2, 0);
    if (activeUsersCount1.count == 0) {
        printf("Group %s doesn't exist.\n", groupName);
        return;
    }
    sendUsername sendUsername1 = {};
    for (int i = 0; i < activeUsersCount1.count; i++) {
        msgrcv(userQueue, &sendUsername1, sizeof(sendUsername) - sizeof(long), 3, 0);
        printf("%s ", sendUsername1.username);
    }
    printf("\n");
}

void joinGroup() {
    command command1 = {1, 5};
    msgsnd(userQueue, &command1, sizeof(command) - sizeof(long), 0);
    sendReceiver sendReceiver1 = {1, "", 0};
    printf("Which group would you like to join?\n");
    char groupName[48];
    readString(groupName);
    strcpy(sendReceiver1.receiver, groupName);
    msgsnd(userQueue, &sendReceiver1, sizeof(sendReceiver) - sizeof(long), 0);
    msgrcv(userQueue, &sendReceiver1, sizeof(sendReceiver) - sizeof(long), 1, 0);
    if (sendReceiver1.isValid == 0) {
        printf("A group with this name doesn't exist.\n");
    } else if (sendReceiver1.isValid == -1) {
        printf("You already are a member of this group.\n");
    } else {
        printf("You successfully joined %s\n", sendReceiver1.receiver);
    }
}

void leaveGroup() {
    command command1 = {1, 7};
    msgsnd(userQueue, &command1, sizeof(command) - sizeof(long), 0);
    sendReceiver sendReceiver1 = {1, "", 0};
    printf("Which group would you like to leave?\n");
    char groupName[48];
    readString(groupName);
    strcpy(sendReceiver1.receiver, groupName);
    msgsnd(userQueue, &sendReceiver1, sizeof(sendReceiver) - sizeof(long), 0);
    msgrcv(userQueue, &sendReceiver1, sizeof(sendReceiver) - sizeof(long), 1, 0);
    if (sendReceiver1.isValid == 0) {
        printf("A group with this name doesn't exist.\n");
    } else if (sendReceiver1.isValid == -1) {
        printf("You are not a member of this group.\n");
    } else {
        printf("You successfully left %s\n.", sendReceiver1.receiver);
    }
}

void blockMessages(blockedUser *blockedUsers, blockedGroup *blockedGroups, int *blockedUsersNumber, int *blockedGroupsNumber) {
    printf("Do you want to block a user (u) or group (g)?\n");
    char userGroup;
    userGroup = getchar();
    if (userGroup == '\n') {
        userGroup = getchar();
    }
    command command1 = {1, 0};
    if (userGroup == 'g') {
        command1.c = 11;
        msgsnd(userQueue, &command1, sizeof(command) - sizeof(long), 0);
        printf("Which group's messages would you like to block?\n");
        char groupName[48];
        readString(groupName);
        semop(semBlockedGroups, &semClose, 1);
        for (int i = 0; i < *blockedGroupsNumber; i++) {
            if (strcmp(blockedGroups[i].name, groupName) == 0) {
                printf("You are already blocking this group!\n");
                semop(semBlockedGroups, &semOpen, 1);
                return;
            }
        }
        semop(semBlockedGroups, &semOpen, 1);
        sendReceiver sendReceiver1 = {2, "", 0};
        strcpy(sendReceiver1.receiver, groupName);
        msgsnd(userQueue, &sendReceiver1, sizeof(sendReceiver) - sizeof(long), 0);
        msgrcv(userQueue, &sendReceiver1, sizeof(sendReceiver) - sizeof(long), 3, 0);
        if (sendReceiver1.isValid == 0) {
            printf("Group %s doesn't exist.\n", groupName);
            return;
        }
        semop(semBlockedGroups, &semClose, 1);
        strcpy(blockedGroups[*blockedGroupsNumber].name, groupName);
        *blockedGroupsNumber = *blockedGroupsNumber + 1;
        semop(semBlockedGroups, &semOpen, 1);
        printf("You are now blocking messages from %s\n", groupName);
    } else if (userGroup == 'u') {
        command1.c = 10;
        msgsnd(userQueue, &command1, sizeof(command) - sizeof(long), 0);
        printf("Which user's messages would you like to block?\n");
        char username[32];
        readString(username);
        semop(semBlockedUsers, &semClose, 1);
        for (int i = 0; i < *blockedUsersNumber; i++) {
            if (strcmp(blockedUsers[i].username, username) == 0) {
                printf("You are already blocking this user!\n");
                semop(semBlockedUsers, &semOpen, 1);
                return;
            }
        }
        semop(semBlockedUsers, &semOpen, 1);
        sendReceiver sendReceiver1 = {2, "", 0};
        strcpy(sendReceiver1.receiver, username);
        msgsnd(userQueue, &sendReceiver1, sizeof(sendReceiver) - sizeof(long), 0);
        msgrcv(userQueue, &sendReceiver1, sizeof(sendReceiver) - sizeof(long), 3, 0);
        if (sendReceiver1.isValid == 0) {
            printf("User %s doesn't exist.\n", username);
            return;
        }
        semop(semBlockedUsers, &semClose, 1);
        strcpy(blockedUsers[*blockedUsersNumber].username, username);
        *blockedUsersNumber = *blockedUsersNumber + 1;
        semop(semBlockedUsers, &semOpen, 1);
        printf("You are now blocking messages from %s\n", username);
    } else {
        printf("Please choose u (group) or g (user)\n");
    }
}

void readString(char text[48]) {
    char buffer[48];
    memset(buffer, 0, sizeof(buffer));
    int size = read(0, buffer, 48);
    strncpy(text, buffer, size);
    text[size-1] = '\0';
}

void readMessage(char message[2048]) {
    char buffer[2048];
    memset(buffer, 0, sizeof(buffer));
    int size = read(0, buffer, 2048);
    strncpy(message, buffer, size);
    message[size-1] = '\0';
}

void closeQueues() {
    command command1 = {1, 8};
    msgsnd(userQueue, &command1, sizeof(command) - sizeof(long), 0);
    semctl(semBlockedUsers, 1, IPC_RMID, NULL);
    semctl(semBlockedGroups, 1, IPC_RMID, NULL);
    shmctl(blockedUsersShmid, IPC_RMID, NULL);
    shmctl(blockedUsersNumberShmid, IPC_RMID, NULL);
    shmctl(blockedGroupsShmid, IPC_RMID, NULL);
    shmctl(blockedGroupsNumberShmid, IPC_RMID, NULL);
}
