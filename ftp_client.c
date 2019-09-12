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

#define FILE_NOT_FOUND      9385615
#define DNT_OVWRT           7544832

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

void get_file_response(int sock, char * param){
    int size, k;
    char *f;
    char buf[100], file_path[100];
    int filehandle;

    recv(sock, &size, sizeof(int), 0);
    printf("Size received: %d\n", size);

    if(size == FILE_NOT_FOUND){
        printf("File not found\n");
        return;
    }

    getcwd(file_path, sizeof(file_path));
    strcat(file_path, "/");
    strcat(file_path, param);

    printf("file_path: %s\n", file_path);

    filehandle = creat(file_path, O_WRONLY);
    error(filehandle, -1, "File creation failed.\n");
    bzero(buf, sizeof(buf));
    strcpy(buf, "chmod 666 ");
    strcat(buf, param);
    // There is no problem using system with the user-input string param
    // (the file name) since this is running on client machine.
    system(buf);

    if(size != 0){
        f = malloc(size);
        recv(sock, f, size, 0);
        k = write(filehandle, f, size);
        error(k, -1, "Reading failed.\n");
        close(filehandle);
        bzero(buf, sizeof(buf));
        strcpy(buf, "cat ");
        strcat(buf, file_path);
        system(buf);
        printf("File successfully copied to local machine\n");
    } else {        // File with 0 bytes
        k = write(filehandle, "", size);
        close(filehandle);
        printf("File successfully copied to local machine. This file was empty\n");
    }
    return;
}

void read_and_ignore(int sock){
    int size;
    char *f;

    recv(sock, &size, sizeof(int), 0);
    printf("Size received: %d\n", size);
    if(size != 0){
        f = malloc(size);
        recv(sock, f, size, 0);
    }

    return;
}

void put_file_in_server(int sock, char * ptr, int ovwrt) {
    struct stat obj;
    int size, filehandle;

    // Check if file exists locally
    printf("file_path: %s\n", ptr);
    printf("access: %d\n", access(ptr, F_OK));
    if(access(ptr, F_OK) != -1){
        // Send amount of bytes to server
        stat(ptr, &obj);
        size = obj.st_size;
        if(!ovwrt)
            size = DNT_OVWRT;
        send(sock, &size, sizeof(int), 0);

        if(ovwrt){
            // Send file to server
            filehandle = open(ptr, O_RDONLY);
            error(filehandle, -1, "Couldn't open file.");
            sendfile(sock, filehandle, NULL, size);
            printf("File successfully copied to server\n");
        } else printf("File was not overwriten\n");
    } else {
        // Send FILE_NOT_FOUND to server
        size = FILE_NOT_FOUND;
        send(sock, &size, sizeof(int), 0);

        printf("File not found\n");
    }
    return;
}

int main(int argc, char *argv[]){
    struct addrinfo hints, *server;
    int sock;
    // unsigned short port = 2121;     // We will use port 2121
    char buf[100], command[100], cmd_name[100], filename[20], *f, ans;
    int k, size, status, exists_in_server;
    int logged = 0, session = 0;
    struct stat obj;
    int filehandle, filesize;
    struct hostent *h;
    struct in_addr address;
    const char delim[2] = " ";
    char * ptr;

    bzero(buf, sizeof(buf));
    while(session == 0 && strcmp(buf, "quit")){
        printf("Type 'open' to start a conection or 'quit' to leave> ");
        bzero(buf, sizeof(buf));
        fgets(buf, sizeof(buf), stdin);
        buf[strlen(buf)-1] = '\0';        // Remove line feed
        printf("Typed: %s\n", buf);
        ptr = NULL;
        ptr = strtok(buf, delim);
        for(int i = 0 ; i < 12 ; i++){
            printf("ptr[%d] = %c : %d\n", i, ptr[i], ptr[i]);
        }
        if(!strcmp(ptr, "open")){
            ptr = strtok(NULL, delim);
            if(ptr != NULL){
                for(int i = 0 ; i < 12 ; i++){
                    printf("ptr[%d] = %c : %d\n", i, ptr[i], ptr[i]);
                }
                memset(&hints, 0, sizeof hints);        // zero structure out
                hints.ai_family = AF_INET;
                hints.ai_socktype = SOCK_STREAM;
                status = getaddrinfo(ptr, "2121", &hints, &server);
                if(status != 0){
                    printf("Couldn't resolve server name. Try another server\n");
                } else{
                    // Create socket. Same as ftp_server.c
                    sock = socket(server->ai_family, server->ai_socktype, server->ai_protocol);
                    error(sock, -1, "Socket creation failed.\n");

                    /* Connect
                    int connect(int sockfd, const struct sockaddr *addr, socklen_t addrlen);
                    Connects the socket referred to by the file descriptor sockfd to the
                    address specified by addr. addrlen argument specifies the size of addr.
                    */
                    k = connect(sock, server->ai_addr, server->ai_addrlen);
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
            } else printf("Type server name\n");
        }
        while(logged){
            // Read command from stdin
            printf("ftp> ");
            bzero(buf, sizeof(buf));
            fgets(buf, sizeof(buf)-1, stdin);
            printf("command read: %s\n", buf);

            strcpy(command, buf);
            strcpy(cmd_name, buf);
            ptr = NULL;
            ptr = strtok(cmd_name, delim);
            printf("cmd_name: %s\n", cmd_name);
            if(iscntrl(cmd_name[strlen(cmd_name)-1]))   // Remove line feed if needed
                cmd_name[strlen(cmd_name)-1] = '\0';

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
            } else if(!strcmp(cmd_name, "mkdir")){
                ptr = strtok(NULL, delim);
                if(ptr == NULL){
                    printf("Specify a directory to create\n");
                } else{
                    recv(sock, buf, sizeof(buf), 0);
                    printf("Server's response: %s\n", buf);
                }
            } else if(!strcmp(cmd_name, "rmdir")){
                ptr = strtok(NULL, delim);
                if(ptr == NULL){
                    printf("Specify a directory to erase\n");
                } else{
                    recv(sock, buf, sizeof(buf), 0);
                    printf("Server's response: %s\n", buf);
                }
            } else if(!strcmp(cmd_name, "delete")){
                /* Receive file containing result and print it */
                receive_and_print_file_response(sock);
            } else if(!strcmp(cmd_name, "get")){
                /* Receive file and store it */
                ptr = strtok(NULL, delim);
                if(ptr != NULL){
                    if(iscntrl(ptr[strlen(ptr)-1]))   // Remove line feed if needed
                        ptr[strlen(ptr)-1] = '\0';
                    printf("access: %d\n", access(ptr, F_OK));
                    if(access(ptr, F_OK) != -1){
                        printf("There is a file with the same name in machine. Do you want to overwrite it? [y/n]\n");
                        ans = 'a';
                        while(ans != 'y' && ans != 'n'){
                            scanf("%c", &ans);
                            getchar();
                        }
                        if(ans == 'y'){
                            get_file_response(sock, ptr);
                        } else if(ans == 'n'){
                            read_and_ignore(sock);
                        }
                    } else {
                        get_file_response(sock, ptr);
                    }
                }
                else {
                    printf("File not specified. Choose a file to get\n");
                }
            } else if(!strcmp(cmd_name, "put")){
                /* Send file to server */
                ptr = strtok(NULL, delim);
                if(ptr != NULL){
                    if(iscntrl(ptr[strlen(ptr)-1]))   // Remove line feed if needed
                        ptr[strlen(ptr)-1] = '\0';
                    // Receive message from server: 0 for file not there
                    // and 1 for file already there
                    recv(sock, &exists_in_server, sizeof(int), 0);
                    printf("File already exists -> %d\n", exists_in_server);
                    if(exists_in_server){
                        printf("There is a file with the same name in the server. Do you want to overwrite it? [y/n]\n");
                        ans = 'a';
                        while(ans != 'y' && ans != 'n'){
                            scanf("%c", &ans);
                            getchar();
                        }
                        if(ans == 'y'){
                            put_file_in_server(sock, ptr, 1);
                        } else if(ans == 'n'){
                            put_file_in_server(sock, ptr, 0);
                        }
                    } else {
                        put_file_in_server(sock, ptr, 1);
                    }
                } else{
                    printf("File not specified. Choose a file to put\n");
                }
            } else {
                printf("Command not found\n");
            }
        }
    }
}
