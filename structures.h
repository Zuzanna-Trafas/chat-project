
#ifndef PROJEKT_STRUCTURES_H
#define PROJEKT_STRUCTURES_H

typedef struct user {
    char login[32];
    char password[32];
    int id;
    short active;
    short tries;
} user;

typedef struct group {
    char name[48];
    int members[10];
    int membersNumber;
} group;

typedef struct blockedUser {
    char username[32];
} blockedUser;

typedef struct blockedGroup {
    char name[48];
} blockedGroup;

typedef struct command {
    long mtype;
    int c;
} command;

typedef struct loginInfo {
    long mtype;
    char login[32];
    char password[32];
} loginInfo;

typedef struct sendUserId {
    long mtype;
    int id;
    int tries;
} sendUserId;

typedef struct message {
    long mtype;
    char text[2048];
    char sender[32];
    char groupName[48];
    short close;
} message;

typedef struct sendReceiver {
    long mtype;
    char receiver[48];
    short isValid;
} sendReceiver;

typedef struct activeUsersCount {
    long mtype;
    int count;
} activeUsersCount;

typedef struct sendUsername {
    long mtype;
    char username[32];
} sendUsername;

typedef struct sendGroupName {
    long mtype;
    char groupName[48];
} sendGroupName;

#endif //PROJEKT_STRUCTURES_H
