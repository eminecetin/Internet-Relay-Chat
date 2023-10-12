
#include <sys/socket.h>
#include <arpa/inet.h>
#include <string.h>

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#define BUFFER_SIZE 1024

int main() {

    int sockfd;
    struct sockaddr_in serverAddr;
    char buffer[BUFFER_SIZE];

    // Soket oluşturma işlemi buradan gerçekleşiyor,,..

    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    
    if (sockfd < 0) {
        perror("Soket oluşturulamadı");
        exit(1);
    }

    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(1234); // Sunucu port numarası
    serverAddr.sin_addr.s_addr = inet_addr("127.0.0.1"); // Sunucu IP adresi

    // Sunucuya bağlan
    if (connect(sockfd, (struct sockaddr*)&serverAddr, sizeof(serverAddr)) < 0) {
        perror("Sunucuya bağlanılamadı");
        exit(1);
    }

    printf("IRC İstemcisi başlatıldı\n");

    while (1) {
        printf("\nKomut girin: ");
        fgets(buffer, BUFFER_SIZE, stdin);
        buffer[strcspn(buffer, "\n")] = '\0';

        // Sunucuya komut gönder
        if (write(sockfd, buffer, strlen(buffer)) < 0) {
            perror("Komut gönderilemedi");
            exit(1);
        }

        // Sunucudan yanıt al
        ssize_t bytesRead = read(sockfd, buffer, BUFFER_SIZE - 1);
        if (bytesRead < 0) {
            //Hata işi içinn
            perror("Yanıt alınamadı");
            exit(1);
        } else if (bytesRead == 0) {
            printf("Sunucu bağlantısı koptu\n");
            break;
        }

        buffer[bytesRead] = '\0';
        printf("Yanıt: %s\n", buffer);
    }

    // Soketi kapat
    close(sockfd);

    return 0;
}
