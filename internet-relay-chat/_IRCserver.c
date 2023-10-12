#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <pthread.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <stdlib.h>
#define BUFFER_SIZE 1024
#define MAX_ROOMLARIM 10

//maxBilgileriim
#define MAX_CLIENTLERİM 10

#define MAX_CLIENTS_PERNUMLARIM 10

pthread_mutex_t roomsLock;//  clientler oluşturulmadan roomslock yapılmalı
typedef struct {
    int socket;
    char nickname[20];
    char currentRoom[20];
} Client;
//
typedef struct {
    char name[20];
    Client* clients[MAX_CLIENTS_PERNUMLARIM];
    int clientCount;

    pthread_mutex_t lock;
} Room;

Client clients[MAX_CLIENTLERİM];
int clientCount = 0;
Room rooms[MAX_ROOMS];
int roomCount = 0;
pthread_mutex_t clientsLock;

void initializeRooms() {
roomCount = 0;
    for (int i = 0; i < MAX_ROOMLARIM; i++) {
        rooms[i].clientCount = 0;
        pthread_mutex_init(&rooms[i].lock, NULL);
    }
}

Room* getRoomByName(const char* name) {
    for (int i = 0; i < roomCount; i++) {
        if (strcmp(rooms[i].name, name) == 0) {
            return &rooms[i];
        }
    }
    return NULL;
}

void sendToRoom(Room* room, const char* message) {
    pthread_mutex_lock(&room->lock);
    for (int i = 0; i < room->clientCount; i++) {
        if (write(room->clients[i]->socket, message, strlen(message)) < 0) {
            perror("Mesaj gönderilemedi");
        }
    }
    pthread_mutex_unlock(&room->lock);
}

void* clientThread(void* arg) {
    Client* client = (Client*)arg;
    char buffer[BUFFER_SIZE];

    while (1) {
        ssize_t bytesRead = read(client->socket, buffer, BUFFER_SIZE);
        if (bytesRead < 0) {
            perror("Mesaj alınamadı");
            break;
        } else if (bytesRead == 0) {
            printf("İstemci bağlantısı koptu: %s\n", client->nickname);
            break;
        }

        buffer[bytesRead] = '\0';
        printf("Alınan mesaj (%s): %s\n", client->nickname, buffer);

        // Komutu işle
        if (strncmp(buffer, "login", 5) == 0) {
            char* nickname = buffer + 6;
            snprintf(client->nickname, sizeof(client->nickname), "%s", nickname);
            printf("Kullanıcı kaydedildi (%s)\n", client->nickname);
        } else if (strncmp(buffer, "logout", 6) == 0) {
            printf("Kullanıcı çıkış yaptı (%s)\n", client->nickname);
            break;
        } else if (strncmp(buffer, "list rooms", 10) == 0) {
            printf("Mevcut sohbet odaları:\n");
            for (int i = 0; i < roomCount; i++) {
                printf("- %s\n", rooms[i].name);
            }
        } else if (strncmp(buffer, "list users", 10) == 0) {
            printf("Geçerli sohbet odasındaki kullanıcılar:\n");
            Room* currentRoom = getRoomByName(client->currentRoom);
            if (currentRoom != NULL) {
                pthread_mutex_lock(&currentRoom->lock);
                for (int i = 0; i < currentRoom->clientCount; i++) {
                    printf("- %s\n", currentRoom->clients[i]->nickname);
                }
                pthread_mutex_unlock(&currentRoom->lock);
            }
        } else if (strncmp(buffer, "enter", 5) == 0) {
            char* roomName = buffer + 6;
            Room* newRoom = getRoomByName(roomName);
            if (newRoom != NULL) {
                Room* oldRoom = getRoomByName(client->currentRoom);
                if (oldRoom != NULL) {
                    pthread_mutex_lock(&oldRoom->lock);
                    for (int i = 0; i < oldRoom->clientCount; i++) {
                        if (oldRoom->clients[i] == client) {
                            for (int j = i; j < oldRoom->clientCount - 1; j++) {
                                oldRoom->clients[j] = oldRoom->clients[j + 1];
                            }
                            oldRoom->clientCount--;
                            break;
                        }
                    }
                    pthread_mutex_unlock(&oldRoom->lock);
                }

                pthread_mutex_lock(&newRoom->lock);
                newRoom->clients[newRoom->clientCount] = client;
                newRoom->clientCount++;
                pthread_mutex_unlock(&newRoom->lock);

                snprintf(client->currentRoom, sizeof(client->currentRoom), "%s", roomName);
                printf("%s, %s odasına giriş yaptı\n", client->nickname, roomName);
            } else {
                printf("%s adında bir oda bulunamadı\n", roomName);
            }
        } else if (strncmp(buffer, "whereami", 8) == 0) {
            printf("İçinde bulunduğunuz oda: %s\n", client->currentRoom);
        } else if (strncmp(buffer, "open", 4) == 0) {
            char* roomName = buffer + 5;
            if (roomCount < MAX_ROOMS) {
                Room* newRoom = getRoomByName(roomName);
                if (newRoom == NULL) {
                    pthread_mutex_lock(&roomsLock);
                    snprintf(rooms[roomCount].name, sizeof(rooms[roomCount].name), "%s", roomName);
                    rooms[roomCount].clientCount = 0;
                    pthread_mutex_init(&rooms[roomCount].lock, NULL);
                    roomCount++;
                    pthread_mutex_unlock(&roomsLock);

                    printf("%s odası oluşturuldu\n", roomName);
                } else {
                    printf("%s adında bir oda zaten mevcut\n", roomName);
                }
            } else {
                printf("Maksimum oda sayısına ulaşıldı\n");
            }
        } else if (strncmp(buffer, "close", 5) == 0) {
            char* roomName = buffer + 6;
            Room* targetRoom = getRoomByName(roomName);
            if (targetRoom != NULL) {
                if (strcmp(targetRoom->name, client->currentRoom) == 0) {
                    pthread_mutex_lock(&roomsLock);
                    for (int i = 0; i < roomCount; i++) {
                        if (strcmp(rooms[i].name, roomName) == 0) {
                            for (int j = i; j < roomCount - 1; j++) {
                                rooms[j] = rooms[j + 1];
                            }
                            roomCount--;
                            break;
                        }
                    }
                    pthread_mutex_unlock(&roomsLock);

                    printf("%s odası kapatıldı\n", roomName);
                } else {
                    printf("%s odasını kapatma yetkiniz yok\n", roomName);
                }
            } else {
                printf("%s adında bir oda bulunamadı\n", roomName);
            }
        }
    }

    // İstemci soketini kapat
    close(client->socket);

    // İstemciyi listeden kaldır
    pthread_mutex_lock(&clientsLock);
    for (int i = 0; i < clientCount; i++) {
        if (clients[i].socket == client->socket) {
            for (int j = i; j < clientCount - 1; j++) {
                clients[j] = clients[j + 1];
            }
            clientCount--;
            break;
        }
    }
    pthread_mutex_unlock(&clientsLock);

    // İstemciyi serbest bırak
    free(client);

    return NULL;
}

int main() {
    initializeRooms();
    int sockfd, newSockfd;
    struct sockaddr_in serverAddr, clientAddr;
    //
    //SockAdr
    socklen_t clientSize;
    pthread_t tid;

    // Soket oluştur
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) {
        perror("Soket oluşturulamadı");
        exit(1);
    }

    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(1234); // Sunucu port numarası
    //sunucu portu
    serverAddr.sin_addr.s_addr = htonl(INADDR_ANY); // Tüm arayüzlere bağlanma islemi buralardan yapılıyore.

    // Soketi adres ile ilişkilendir
    if (bind(sockfd, (struct sockaddr*)&serverAddr, sizeof(serverAddr)) < 0) {
        perror("Soket ile adres ilişkilendirilemedi");
        exit(1);
    }


    // Bağlantıları dinle
    if (listen(sockfd, 10) < 0) {
        perror("Bağlantılar dinlenemedi");
        exit(1);
    }

    printf("IRC Sunucusu başlatıldı\n");

    while (1) {
        clientSize = sizeof(clientAddr);

        // Yeni bağlantıyı kabul et
        newSockfd = accept(sockfd, (struct sockaddr*)&clientAddr, &clientSize);
        if (newSockfd < 0) {
            perror("Bağlantı kabul edilemedi");
            //edilmedi....
            exit(1);
        }

        // Yeni istemci için bellekte yer ayır
        Client* newClient = (Client*)malloc(sizeof(Client));
        newClient->socket = newSockfd;
        snprintf(newClient->nickname, sizeof(newClient->nickname), "%s", "Misafir");
        snprintf(newClient->currentRoom, sizeof(newClient->currentRoom), "%s", "Genel");

        // İstemci listesine ekle
        pthread_mutex_lock(&clientsLock);
        clients[clientCount] = *newClient;
        clientCount++;
        pthread_mutex_unlock(&clientsLock);

        printf("Yeni istemci bağlandı\n");

        // İstemci için yeni bir iş parçacığı oluştur
        if (pthread_create(&tid, NULL, clientThread, (void*)newClient) != 0) {
            perror("İstemci iş parçacığı oluşturulamadı");
            exit(1);
        }
    }

    // Soketi kapatmak içinn...
    close(sockfd);

    return 0;
}
