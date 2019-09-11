/*FTP Client*/
#include<sys/socket.h>
#include<netinet/in.h>
#include<string.h>
#include<stdio.h>
#include<stdlib.h>
#include<errno.h>
#include<ctype.h>

/*for getting file size using stat()*/
#include<sys/stat.h>

/*for sendfile()*/
#include<sys/sendfile.h>

/*for O_RDONLY*/
#include<fcntl.h>

/*for gethostbyname*/
#include<netdb.h>

/*for inet_aton*/
#include <arpa/inet.h>

/* for read and write */
#include <unistd.h>

void error(int a, int b, char * msg){
    if(a == b){
        printf("\nERROR: %s", msg);
        printf("Errno: %s\n", strerror(errno));
        exit(1);
    }

    return;
}

void receive_and_print_file_response(int sock){
    int size, k;
    char *f;
    int filehandle;

    recv(sock, &size, sizeof(int), 0);
    printf("Size received: %d\n", size);
    if(size != 0){
        f = malloc(size);
        recv(sock, f, size, 0);
        filehandle = creat("temp.txt", O_WRONLY);
        error(filehandle, -1, "Creating temp.txt failed.\n");
        system("chmod 777 temp.txt");
        k = write(filehandle, f, size);
        error(k, -1, "Reading failed.\n");
        close(filehandle);
        system("cat temp.txt");
        k = remove("temp.txt");
        error(k, -1, "temp.txt remove failed.\n");
    } else printf("\n");
}

int main(int argc, char *argv[]){
    struct sockaddr_in server;
    int sock;
    unsigned short port = 2121;     // We will use port 2121
    char buf[100], command[100], cmd_name[100], filename[20], *f;
    int k, size, status;
    int logged = 0, session = 0;
    struct stat obj;
    int filehandle, filesize;
    struct hostent *h;
    struct in_addr address;
    const char delim[2] = " ";
    char * ptr;

    bzero(buf, sizeof(buf));
    while(session == 0 && strcmp(buf, "quit")){
        // Create socket. Same as ftp_server.c
        sock = socket(AF_INET, SOCK_STREAM, 0);
        error(sock, -1, "Socket creation failed.\n");

        memset(&server, '\0', sizeof(server));          // zero structure out
        server.sin_family = AF_INET;                    // match socket call
        server.sin_port = htons(port);                  // bind to port
        server.sin_addr.s_addr = inet_addr("127.0.0.1");

        printf("Type 'open' to start a conection or 'quit' to leave> ");
        bzero(buf, sizeof(buf));
        fgets(buf, sizeof(buf), stdin);
        buf[strlen(buf)-1] = '\0';        // Remove line feed
        printf("Typed: %s\n", buf);
        if(!strcmp(buf, "open")){
            /* Connect
            int connect(int sockfd, const struct sockaddr *addr, socklen_t addrlen);
            Connects the socket referred to by the file descriptor sockfd to the
            address specified by addr. addrlen argument specifies the size of addr.
            */
            k = connect(sock, (struct sockaddr*)&server, sizeof(server));
            error(k, -1, "Connect failed.\n");
            session = 1;
            printf("\nConnected.\n\n");

            printf("Type your username > ");
            bzero(buf, sizeof(buf));
            fgets(buf, sizeof(buf)-1, stdin);
            printf("username typed: %s\n", buf);

            printf("\nSending username to server: %s\n", buf);
            k = write(sock, buf, sizeof(buf));
            error(k, -1, "Sending failed.\n");

            printf("Type your password > ");
            bzero(buf, sizeof(buf));
            fgets(buf, sizeof(buf)-1, stdin);
            printf("Password typed: %s\n", buf);

            printf("\nSending password to server: %s\n", buf);
            k = write(sock, buf, sizeof(buf));
            error(k, -1, "Sending failed.\n");

            bzero(buf, sizeof(buf));
            k = read(sock, buf, sizeof(buf));
            error(k, -1, "Reading failed.\n");

            printf("Server's response: %s\n", buf);

            if(!strcmp(buf, "230"))
                logged = 1;
            else if (!strcmp(buf, "530")){
                session = 0;
                shutdown(sock, SHUT_RDWR);
                close(sock);
            }
        }
        while(logged){
            // Read command from stdin
            printf("ftp> ");
            bzero(buf, sizeof(buf));
            fgets(buf, sizeof(buf)-1, stdin);
            printf("command read: %s\n", buf);

            strcpy(command, buf);
            strcpy(cmd_name, buf);
            ptr = strtok(cmd_name, delim);
            printf("cmd_name: %s\n", cmd_name);
            if(iscntrl(cmd_name[strlen(cmd_name)-1]))   // Remove line feed if needed
                cmd_name[strlen(cmd_name)-1] = '\0';    // Remove line feed

            printf("\nsending buf: %s\n", buf);
            k = write(sock, buf, sizeof(buf));
            error(k, -1, "Sending failed.\n");

            if(!strcmp(cmd_name, "close")){
                bzero(buf, sizeof(buf));
                k = read(sock, buf, sizeof(buf));
                error(k, -1, "Reading failed.\n");
                printf("Server's response: %s\n", buf);
                logged = 0;
                session = 0;
                shutdown(sock, SHUT_RDWR);
                close(sock);
            } else if(!strcmp(cmd_name, "quit")){
                bzero(buf, sizeof(buf));
                k = read(sock, buf, sizeof(buf));
                error(k, -1, "Reading failed.\n");
                printf("Server's response: %s\n", buf);
                logged = 0;
                session = 0;
                shutdown(sock, SHUT_RDWR);
                close(sock);
                return 0;               // kill the client
            } else if(!strcmp(cmd_name, "pwd")){
                bzero(buf, sizeof(buf));
                k = read(sock, buf, sizeof(buf));
                error(k, -1, "Reading failed.\n");
                printf("Server's response: %s\n", buf);
            } else if(!strcmp(cmd_name, "cd")){
                bzero(buf, sizeof(buf));
                k = read(sock, buf, sizeof(buf));
                error(k, -1, "Reading failed.\n");
                printf("Server's response: %s\n", buf);
            } else if(!strcmp(cmd_name, "ls")){
                /* Receive file containing result and print it */
                receive_and_print_file_response(sock);
            } else if(!strcmp(cmd_name, "delete")){
                /* Receive file containing result and print it */
                receive_and_print_file_response(sock);
            } else {
                printf("Command not found\n");
            }
        }
    }

    // while(1){
    //     // Read command form stdin
    //     printf("ftp> ");
    //     bzero(buf, sizeof(buf));
    //     fgets(buf, sizeof(buf)-1, stdin);
    //     printf("command read: %s\n", buf);
    //
    //     // Extract command name from input
    //     bzero(cmd_name, sizeof(cmd_name));
    //     strcpy(cmd_name, buf);
    //     ptr = strtok(cmd_name, delim);
    //     printf("cmd_name: %s\n", cmd_name);
    //
    //     // pwd
    //     if(!strcmp(cmd_name, "pwd")){
    //         printf("\nsending buf: %s\n", buf);
    //         k = write(sock, buf, sizeof(buf));
    //         error(k, -1, "Sending failed.\n");
    //
    //         bzero(buf, sizeof(buf));
    //         k = read(sock, buf, sizeof(buf)-1);
    //         error(k, -1, "Reading failed.\n");
    //
    //         printf("Server's response: %s\n", buf);
    //     }
    //     else printf("Not implemented yet\n");
    // }

    // int i = 1;
    // while(1){
    //     printf("Enter a choice:\n1- get\n2- put\n3- pwd\n4- ls\n5- cd\n6- quit\n");
    //     scanf("%d", &choice);
    //     switch(choice){
    //         case 1:
    //             printf("Enter filename to get: ");
    //             scanf("%s", filename);
    //             strcpy(buf, "get ");
    //             strcat(buf, filename);
    //             send(sock, buf, 100, 0);
    //             recv(sock, &size, sizeof(int), 0);
    //             if(!size){
    //                 printf("No such file on the remote directory\n\n");
    //                 break;
    //             }
    //             f = malloc(size);
    //             recv(sock, f, size, 0);
    //             while(1){
    //                 filehandle = open(filename, O_CREAT | O_EXCL | O_WRONLY, 0666);
    //                 if(filehandle == -1){
    //                     sprintf(filename + strlen(filename), "%d", i);//needed only if same directory is used for both server and client
    //                 }
    //                 else break;
    //             }
    //             // fwrite(filehandle, f, size, 0);
    //             // close(filehandle);
    //             strcpy(buf, "cat ");
    //             strcat(buf, filename);
    //             system(buf);
    //             break;
    //         case 2:
    //             printf("Enter filename to put to server: ");
    //             scanf("%s", filename);
    //             filehandle = open(filename, O_RDONLY);
    //             if(filehandle == -1){
    //                 printf("No such file on the local directory\n\n");
    //                 break;
    //             }
    //             strcpy(buf, "put ");
    //             strcat(buf, filename);
    //             send(sock, buf, 100, 0);
    //             stat(filename, &obj);
    //             size = obj.st_size;
    //             send(sock, &size, sizeof(int), 0);
    //             sendfile(sock, filehandle, NULL, size);
    //             recv(sock, &status, sizeof(int), 0);
    //             if(status)
    //                 printf("File stored successfully\n");
    //             else
    //                 printf("File failed to be stored to remote machine\n");
    //             break;
    //     }
    // }
}
