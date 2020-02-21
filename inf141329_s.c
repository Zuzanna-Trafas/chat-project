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

struct sembuf semOpen = {0, 1, 0};
struct sembuf semClose = {0, -1, 0};
static int sigint = 0;

void closeQueues(int x);

void closeClient(int ipc, int clientId);

void readConfig(user *users, group *groups, int *userNumber, int *groupNumber);

void createUser(char *line, user *users, int userId);

void createGroup(char *line, user *users, group *groups, int groupId, int userNumber);

void readNumbers(char *line, int *userNumber, int *groupNumber);

int login(user *users, int userNumber, int ipc);

void sendMessage(int clientQueue, int ipc, user *users, int userNumber, char *login, int semUsers);

void
sendGroupMessage(int clientQueue, int ipc, int clientId, user *users, int userNumber, group *groups, int groupNumber,
                 char *login, int semUsers, int semGroups);

void logout(int ipc, user *users, int userNumber, int clientId, int semUsers);

void sendActiveUsers(int clientQueue, user *users, int userNumber, int semUsers);

void sendGroups(int clientQueue, group *groups, int groupNumber, int semGroups);

void sendGroupMembers(int clientQueue, user *users, group *groups, int userNumber, int groupNumber, int semUsers, int semGroups);

void joinGroup(int clientQueue, int clientId, group *groups, int groupNumber, int semGroups);

void leaveGroup(int clientQueue, int clientId, group *groups, int groupNumber, int semGroups);

void blockUser(int clientQueue, user *users, int userNumber, int semUsers);

void blockGroup(int clientQueue, group *groups, int groupNumber, int semGroups);


int main() {
    signal(SIGINT, closeQueues);

    user *users;
    int shmidUsers = shmget(1500, 30 * sizeof(user), 0666 | IPC_CREAT);
    users = (user *) shmat(shmidUsers, 0, 0);
    int semUsers = semget(1600, 1, 0666 | IPC_CREAT);

    group *groups;
    int shmidGroups = shmget(1501, 10 * sizeof(group), 0666 | IPC_CREAT);
    groups = (group *) shmat(shmidGroups, 0, 0);
    int semGroups = semget(1601, 1, 0666 | IPC_CREAT);

    semop(semUsers, &semOpen, 1);
    semop(semGroups, &semOpen, 1);

    int userNumber;
    int groupNumber;

    readConfig(users, groups, &userNumber, &groupNumber);

    int key = 1000;
    int ipc = msgget(key, 0666 | IPC_CREAT);

    while (1) {
        int clientId = login(users, userNumber, ipc);
        if (sigint == 1) {
            printf("\nSIGINT: Closing.\n");
            msgctl(ipc, IPC_RMID, NULL);
            semctl(semUsers, 1, IPC_RMID, NULL);
            semctl(semGroups, 1, IPC_RMID, NULL);
            shmctl(shmidUsers, IPC_RMID, NULL);
            shmctl(shmidGroups, IPC_RMID, NULL);
            exit(0);
        }
        if (clientId == 0) {
            continue;
        } else {
            int f = fork();
            if (f == 0) {
                int clientQueue = msgget(1000 + clientId, 0666 | IPC_CREAT);
                char clientLogin[32];
                for (int u = 0; u < userNumber; u++) {
                    if (users[u].id == clientId) {
                        strcpy(clientLogin, users[u].login);
                    }
                }
                while (1) {
                    if (sigint == 1) {
                        msgctl(clientQueue, IPC_RMID, NULL);
                        exit(0);
                    }
                    command command1 = {};
                    msgrcv(clientQueue, &command1, sizeof(command1) - sizeof(long), 1, 0);
                    switch (command1.c) {
                        case 1:
                            sendMessage(clientQueue, ipc, users, userNumber, clientLogin, semUsers);
                            break;
                        case 2:
                            sendGroupMessage(clientQueue, ipc, clientId, users, userNumber, groups, groupNumber,
                                             clientLogin, semUsers, semGroups);
                            break;
                        case 3:
                            sendActiveUsers(clientQueue, users, userNumber, semUsers);
                            break;
                        case 4:
                            sendGroups(clientQueue, groups, groupNumber, semGroups);
                            break;
                        case 5:
                            joinGroup(clientQueue, clientId, groups, groupNumber, semGroups);
                            break;
                        case 6:
                            sendGroupMembers(clientQueue, users, groups, userNumber, groupNumber, semUsers, semGroups);
                            break;
                        case 7:
                            leaveGroup(clientQueue, clientId, groups, groupNumber, semGroups);
                            break;
                        case 8:
                            closeClient(ipc, clientId);
                        case 9:
                            logout(ipc, users, userNumber, clientId, semUsers);
                            break;
                        case 10:
                            blockUser(clientQueue, users, userNumber, semUsers);
                            break;
                        case 11:
                            blockGroup(clientQueue, groups, groupNumber, semGroups);
                            break;
                    }
                }
            }
        }
    }
}

void readConfig(user *users, group *groups, int *userNumber, int *groupNumber) {
    int userId = 0; // user position in list
    int groupId = 0; // group position in list

    char buf[1];
    char line[512] = "";
    int fd, i = 0;

    fd = open("config.txt", O_RDONLY);

    if (fd == -1) {
        perror("Error. Could not open configuration file.");
        exit(1);
    }

    while (read(fd, buf, 1) > 0) {
        if (buf[0] != '\n') {
            line[i] = buf[0];
            i++;
        } else {
            line[i] = ' ';
            if (line[0] == 'u') {
                createUser(line, users, userId);
                userId++;
            } else if (line[0] == 'g') {
                createGroup(line, users, groups, groupId, *userNumber);
                groupId++;
            } else {
                // read userNumber and groupNumber
                readNumbers(line, userNumber, groupNumber);
            }
            memset(line, 0, strlen(line)); // cleaning string
            i = 0;
        }
    }
    printf("Users and groups created.\n");
    close(fd);
}

void createUser(char *line, user *users, int userId) {
    static user newUser;
    char newLogin[32] = "";
    char newPassword[32] = "";
    int newId = 0;

    int which = 1; // which information: 1 - login, 2 - password, 3 - id
    int iter = 0;

    for (int i = 2; i < strlen(line); i++) {
        if (*(line + i) != ' ') {
            if (which == 1) {
                newLogin[iter] = *(line + i);
                iter++;
            } else if (which == 2) {
                newPassword[iter] = *(line + i);
                iter++;
            } else if (which == 3) {
                newId *= 10;
                newId += *(line + i) - '0';
            }
        } else {
            if (which == 1) {
                strcpy(newUser.login, newLogin);
                iter = 0;
            } else if (which == 2) {
                strcpy(newUser.password, newPassword);
            } else if (which == 3) {
                newUser.id = newId;
            }
            which++;
        }
        newUser.active = 0;
        newUser.tries = 0;
        users[userId] = newUser;
    }
}

void createGroup(char *line, user *users, group *groups, int groupId, int userNumber) {
    static group newGroup;
    memset(newGroup.members, 0, sizeof(newGroup.members));
    char newName[48] = "";
    int newMemberId = 0;

    int which = 1; // which information: 1 - name, 2 - user id's
    int iter = 0;

    for (int i = 2; i < strlen(line); i++) {
        if (*(line + i) != ' ') {
            if (which == 1) {
                newName[iter] = *(line + i);
                iter++;
            } else {
                newMemberId *= 10;
                newMemberId += *(line + i) - '0';
            }
        } else {
            if (which == 1) {
                strcpy(newGroup.name, newName);
            } else {
                for (int u = 0; u < userNumber; u++) {
                    if (users[u].id == newMemberId) {
                        newGroup.members[which - 2] = newMemberId;
                        break;
                    }
                }
                newMemberId = 0;
            }
            which++;
        }
    }
    newGroup.membersNumber = which - 2;
    groups[groupId] = newGroup;
}

void readNumbers(char *line, int *userNumber, int *groupNumber) {
    int number = 0;
    int isUserNumber = 1;
    for (char *c = line; *c != 0; ++c) {
        if (*c == ' ') {
            if (isUserNumber == 1) {
                *userNumber = number;
                isUserNumber = 0;
            } else {
                *groupNumber = number;
            }
            number = 0;
        } else {
            number *= 10;
            number += c[0] - '0';
        }
    }
}

int login(user *users, int userNumber, int ipc) {
    loginInfo loginInfo1 = {};
    sendUserId sendUserId1 = {3, 0, 0};
    msgrcv(ipc, &loginInfo1, sizeof(loginInfo) - sizeof(long), 2, 0);
    if (strcmp(loginInfo1.login, "") == 0) {
        return 0;
    }
    int clientId = 0;
    for (int u = 0; u < userNumber; u++) {
        if (strcmp(users[u].login, loginInfo1.login) == 0) {
            if (strcmp(users[u].password, loginInfo1.password) == 0 && users[u].tries < 3) {
                users[u].tries = 0;
                users[u].active = 1;
                clientId = users[u].id;
                printf("User %s logged.\n", users[u].login);
            } else {
                users[u].tries++;
                printf("User %s tried to log in. Wrong password, %d tries left.\n", users[u].login, 3 - users[u].tries);
            }
            sendUserId1.tries = users[u].tries;
            break;
        }
    }
    if (clientId == 0) {
        printf("User %s not found\n", loginInfo1.login);
    }
    sendUserId1.id = clientId;
    msgsnd(ipc, &sendUserId1, sizeof(sendUserId) - sizeof(long), 0);
    return clientId;
}

void sendMessage(int clientQueue, int ipc, user *users, int userNumber, char *login, int semUsers) {
    sendReceiver getReceiver = {};
    msgrcv(clientQueue, &getReceiver, sizeof(sendReceiver) - sizeof(long), 2, 0);
    int receiverId = 0;
    semop(semUsers, &semClose, 1);
    for (int u = 0; u < userNumber; u++) {
        if (strcmp(users[u].login, getReceiver.receiver) == 0) {
            if (users[u].active == 0) {
                receiverId = -1;
            } else {
                receiverId = users[u].id;
            }
            break;
        }
    }
    semop(semUsers, &semOpen, 1);
    if (receiverId > 0) {
        getReceiver.isValid = 1;
        msgsnd(clientQueue, &getReceiver, sizeof(sendReceiver) - sizeof(long), 0);
    } else if (receiverId == -1) {
        printf("Receiver %s not active.\n", getReceiver.receiver);
        getReceiver.isValid = (short) receiverId;
        msgsnd(clientQueue, &getReceiver, sizeof(sendReceiver) - sizeof(long), 0);
        return;
    } else {
        printf("Receiver %s not found.\n", getReceiver.receiver);
        getReceiver.isValid = (short) receiverId;
        msgsnd(clientQueue, &getReceiver, sizeof(sendReceiver) - sizeof(long), 0);
        return;
    }
    message message1 = {};
    msgrcv(clientQueue, &message1, sizeof(message) - sizeof(long), 3, 0);
    message1.mtype = 1000 + receiverId;
    strcpy(message1.sender, login);
    msgsnd(ipc, &message1, sizeof(message) - sizeof(long), 0);
    printf("Message from %s to %s sent.\n", message1.sender, getReceiver.receiver);
}

void
sendGroupMessage(int clientQueue, int ipc, int clientId, user *users, int userNumber, group *groups, int groupNumber,
                 char *login, int semUsers, int semGroups) {
    sendReceiver getReceiver = {};
    msgrcv(clientQueue, &getReceiver, sizeof(sendReceiver) - sizeof(long), 2, 0);
    int groupIndex = 0;
    semop(semGroups, &semClose, 1);
    for (int g = 0; g < groupNumber; g++) {
        if (strcmp(groups[g].name, getReceiver.receiver) == 0) {
            getReceiver.isValid = 1;
            groupIndex = g;
            break;
        }
    }
    semop(semGroups, &semOpen, 1);
    msgsnd(clientQueue, &getReceiver, sizeof(sendReceiver) - sizeof(long), 0);
    if (getReceiver.isValid == 0) {
        printf("Group %s not found.\n", getReceiver.receiver);
        return;
    }
    message message1 = {};
    msgrcv(clientQueue, &message1, sizeof(message) - sizeof(long), 3, 0);
    strcpy(message1.sender, login);
    semop(semUsers, &semClose, 1);
    semop(semGroups, &semClose, 1);
    for (int i = 0; i < groups[groupIndex].membersNumber; i++) {
        for (int u = 0; u < userNumber; u++) {
            if (groups[groupIndex].members[i] == users[u].id && users[u].id != clientId) {
                if (users[u].active == 1) {
                    message1.mtype = 1000 + users[u].id;
                    msgsnd(ipc, &message1, sizeof(message) - sizeof(long), 0);
                    printf("Message from %s from %s to %s sent.\n", message1.sender, getReceiver.receiver, users[u].login);
                }
            }
        }
    }
    printf("All messages to group %s sent.\n", getReceiver.receiver);
    semop(semGroups, &semOpen, 1);
    semop(semUsers, &semOpen, 1);
}

void logout(int ipc, user *users, int userNumber, int clientId, int semUsers) {
    message closeMessage = {1000 + clientId, "", "", "", 1};
    msgsnd(ipc, &closeMessage, sizeof(message) - sizeof(long), 0);
    semop(semUsers, &semClose, 1);
    for (int u = 0; u < userNumber; u++) {
        if (users[u].id == clientId) {
            users[u].active = 0;
            break;
        }
    }
    semop(semUsers, &semOpen, 1);
    printf("User with id: %d logged out.\n", clientId);
}

void sendActiveUsers(int clientQueue, user *users, int userNumber, int semUsers) {
    int activeUsersNumber = 0;
    semop(semUsers, &semClose, 1);
    for (int u = 0; u < userNumber; u++) {
        if (users[u].active == 1) {
            activeUsersNumber++;
        }
    }
    activeUsersCount activeUsersCount1 = {2, activeUsersNumber};
    msgsnd(clientQueue, &activeUsersCount1, sizeof(activeUsersCount) - sizeof(long), 0);
    sendUsername sendUsername1 = {3, ""};
    printf("Sending active users:\n");
    for (int u = 0; u < userNumber; u++) {
        if (users[u].active == 1) {
            strcpy(sendUsername1.username, users[u].login);
            msgsnd(clientQueue, &sendUsername1, sizeof(sendUsername) - sizeof(long), 0);
            printf("%s ", users[u].login);
        }
    }
    printf("\nAll active users sent.\n");
    semop(semUsers, &semOpen, 1);
}

void sendGroups(int clientQueue, group *groups, int groupNumber, int semGroups) {
    semop(semGroups, &semClose, 1);
    activeUsersCount activeUsersCount1 = {2, groupNumber};
    msgsnd(clientQueue, &activeUsersCount1, sizeof(activeUsersCount) - sizeof(long), 0);
    sendGroupName sendGroupName1 = {3, ""};
    printf("Sending all group names:\n");
    for (int g = 0; g < groupNumber; g++) {
        strcpy(sendGroupName1.groupName, groups[g].name);
        msgsnd(clientQueue, &sendGroupName1, sizeof(sendGroupName) - sizeof(long), 0);
        printf("%s ", groups[g].name);
    }
    printf("\nAll groups sent.\n");
    semop(semGroups, &semOpen, 1);
}

void sendGroupMembers(int clientQueue, user *users, group *groups, int userNumber, int groupNumber, int semUsers, int semGroups) {
    sendGroupName getGroupName1 = {};
    msgrcv(clientQueue, &getGroupName1, sizeof(getGroupName1) - sizeof(long), 4, 0);
    semop(semGroups, &semClose, 1);
    int groupIndex = 0;
    activeUsersCount activeUsersCount1 = {2, 0};
    for (int g = 0; g < groupNumber; g++) {
        if (strcmp(groups[g].name, getGroupName1.groupName) == 0) {
            activeUsersCount1.count = groups[g].membersNumber;
            groupIndex = g;
            break;
        }
    }
    if (activeUsersCount1.count == 0) {
        printf("Group %s not found.\n", getGroupName1.groupName);
        msgsnd(clientQueue, &activeUsersCount1, sizeof(activeUsersCount) - sizeof(long), 0);
        semop(semGroups, &semOpen, 1);
        return;
    }
    msgsnd(clientQueue, &activeUsersCount1, sizeof(activeUsersCount) - sizeof(long), 0);
    sendUsername sendUsername1 = {3, ""};
    semop(semUsers, &semClose, 1);
    printf("Sending %s members:\n", groups[groupIndex].name);
    for (int i = 0; i < groups[groupIndex].membersNumber; i++) {
        for (int u = 0; u < userNumber; u++) {
            if (users[u].id == groups[groupIndex].members[i]) {
                strcpy(sendUsername1.username, users[u].login);
                msgsnd(clientQueue, &sendUsername1, sizeof(sendUsername) - sizeof(long), 0);
                printf("%s ", users[u].login);
            }
        }
    }
    printf("\nAll members sent.\n");
    semop(semUsers, &semOpen, 1);
    semop(semGroups, &semOpen, 1);
}

void joinGroup(int clientQueue, int clientId, group *groups, int groupNumber, int semGroups) {
    sendReceiver getReceiver = {};
    msgrcv(clientQueue, &getReceiver, sizeof(sendReceiver) - sizeof(long), 1, 0);
    int groupIndex = -1;
    semop(semGroups, &semClose, 1);
    for (int g = 0; g < groupNumber; g++) {
        if (strcmp(groups[g].name, getReceiver.receiver) == 0) {
            getReceiver.isValid = 1;
            groupIndex = g;
            break;
        }
    }
    if (groupIndex != -1) {
        for (int g = 0; g < groups[groupIndex].membersNumber; g++) {
            if (groups[groupIndex].members[g] == clientId) {
                getReceiver.isValid = -1;
            }
        }
    }
    semop(semGroups, &semOpen, 1);
    msgsnd(clientQueue, &getReceiver, sizeof(sendReceiver) - sizeof(long), 0);
    if (getReceiver.isValid == 0) {
        printf("Group %s not found.\n", getReceiver.receiver);
        return;
    } else if (getReceiver.isValid == -1) {
        printf("User with id: %d is already a member of group %s\n", clientId, getReceiver.receiver);
        return;
    }
    semop(semGroups, &semClose, 1);
    groups[groupIndex].members[groups[groupIndex].membersNumber] = clientId;
    groups[groupIndex].membersNumber += 1;
    semop(semGroups, &semOpen, 1);
    printf("User with id: %d added to group %s\n", clientId, getReceiver.receiver);
}


void leaveGroup(int clientQueue, int clientId, group *groups, int groupNumber, int semGroups) {
    sendReceiver getReceiver = {};
    msgrcv(clientQueue, &getReceiver, sizeof(sendReceiver) - sizeof(long), 1, 0);
    int groupIndex = 0;
    int userIndex = 0;
    semop(semGroups, &semClose, 1);
    for (int g = 0; g < groupNumber; g++) {
        if (strcmp(groups[g].name, getReceiver.receiver) == 0) {
            getReceiver.isValid = -1;
            for (int u = 0; u < groups[g].membersNumber; u++) {
                if (groups[g].members[u] == clientId) {
                    getReceiver.isValid = 1;
                    userIndex = u;
                    break;
                }
            }
            groupIndex = g;
            break;
        }
    }
    semop(semGroups, &semOpen, 1);
    msgsnd(clientQueue, &getReceiver, sizeof(sendReceiver) - sizeof(long), 0);
    if (getReceiver.isValid == 0) {
        printf("Group %s not found.\n", getReceiver.receiver);
        return;
    } else if (getReceiver.isValid == -1) {
        printf("User with id: %d is not a member of group %s.\n", clientId, getReceiver.receiver);
        return;
    }
    semop(semGroups, &semClose, 1);
    groups[groupIndex].membersNumber--;
    for (int u = userIndex; u < groups[groupIndex].membersNumber; u++) {
        groups[groupIndex].members[u] = groups[groupIndex].members[u + 1];
    }
    semop(semGroups, &semOpen, 1);
    printf("User with id: %d deleted from group %s\n", clientId, getReceiver.receiver);
}

void blockUser(int clientQueue, user *users, int userNumber, int semUsers) {
    sendReceiver sendReceiver1 = {};
    msgrcv(clientQueue, &sendReceiver1, sizeof(sendReceiver) - sizeof(long), 2, 0);
    sendReceiver1.mtype = 3;
    semop(semUsers, &semClose, 1);
    for (int u = 0; u < userNumber; u++) {
        if (strcmp(users[u].login, sendReceiver1.receiver) == 0) {
            sendReceiver1.isValid = 1;
            break;
        }
    }
    semop(semUsers, &semOpen, 1);
    msgsnd(clientQueue, &sendReceiver1, sizeof(sendReceiver) - sizeof(long), 0);
    if (sendReceiver1.isValid == 0) {
        printf("User %s doesn't exist.\n", sendReceiver1.receiver);
    } else {
        printf("User %s blocked.\n", sendReceiver1.receiver);
    }
}

void blockGroup(int clientQueue, group *groups, int groupNumber, int semGroups) {
    sendReceiver sendReceiver1 = {};
    msgrcv(clientQueue, &sendReceiver1, sizeof(sendReceiver) - sizeof(long), 2, 0);
    sendReceiver1.mtype = 3;
    semop(semGroups, &semClose, 1);
    for (int g = 0; g < groupNumber; g++) {
        if (strcmp(groups[g].name, sendReceiver1.receiver) == 0) {
            sendReceiver1.isValid = 1;
            break;
        }
    }
    semop(semGroups, &semOpen, 1);
    msgsnd(clientQueue, &sendReceiver1, sizeof(sendReceiver) - sizeof(long), 0);
    if (sendReceiver1.isValid == 0) {
        printf("Group %s doesn't exist.\n", sendReceiver1.receiver);
    } else {
        printf("Group %s blocked.\n", sendReceiver1.receiver);
    }
}

void closeClient(int ipc, int clientId) {
    message message1 = {1000 + clientId, "", "xxx", "", -1};
    msgsnd(ipc, &message1, sizeof(message) - sizeof(long), 0);
}

void closeQueues(int x) {
    sigint = 1;
}


