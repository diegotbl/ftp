/*FTP server*/
#include<sys/socket.h>
#include<netinet/in.h>
#include<string.h>
#include<stdio.h>
#include<stdlib.h>
#include<ctype.h>

/*for getting file size using stat()*/
#include<sys/stat.h>

/*for sendfile()*/
#include<sys/sendfile.h>

/*for O_RDONLY*/
#include<fcntl.h>

/* for read and write */
#include<unistd.h>

/* for thread handling */
#include<pthread.h>

typedef struct {
    int sock;
    char base_dir[50];
    char original_dir[50];
} client_info;

void error(int a, int b, char * msg){
    if(a == b){
        printf("\nERROR: %s", msg);
        exit(1);
    }
    return;
}

int auth(char * username, char * password, FILE * cred_file){
    /* Checks if (username, password) is in cred_file */
    char line[100], user[100];
    char * ptr;
    const char delim[2] = " ";

    while(!feof(cred_file)){
        fgets(line, 100, cred_file);
        // Remove line feed if needed
        if(iscntrl(line[strlen(line)-1]))
            line[strlen(line)-1] = '\0';
        strcpy(user, line);
        ptr = strtok(user, delim);
        printf("%s user read from file\n", user);
        ptr = strtok(NULL, delim);
        printf("ptr: %s\n", ptr);
        if(!strcmp(username, user)){
            printf("User found: %s\n", user);
            if(!strcmp(password, ptr)){
                printf("User authenticated\n");
                return 1;
            }
            else {
                printf("Password incorrect for user %s\n", user);
                return 0;
            }
        }
    }
    printf("User NOT found. Retry.\n");
    return 0;
}

void * handle_client(void * args){
    int err, i, size, len, c, j;
    int logged = 0;
    char buf[100], command[100];
    char * ptr;                         // pointer to split string
    char * param;                   // result of command spliting
    char cmd_name[100];
    const char delim[2] = " ";          // split delimiter
    client_info * info = args;
    char username[100], pswd[100];
    FILE * cred_file;
    int retval = 0;

    printf("New thread. Logging in.\n");

    // Opening cred_file for client authentication
    chdir(info->original_dir);
    cred_file = fopen("credentials.txt", "r");
    chdir(info->base_dir);

    // Waiting username from client
    printf("Waiting for username.\n");
    bzero(buf, sizeof(buf));
    err = read(info->sock, buf, sizeof(buf));
    error(err, -1, "Reading failed.\n");
    printf("Username: %s\n", buf);
    strcpy(username, buf);
    username[strlen(username)-1] = '\0';        // Remove line feed

    // Waiting password from client
    printf("Waiting for password.\n");
    bzero(buf, sizeof(buf));
    err = read(info->sock, buf, sizeof(buf));
    error(err, -1, "Reading failed.\n");
    printf("Password: %s\n", buf);
    strcpy(pswd, buf);
    pswd[strlen(pswd)-1] = '\0';        // Remove line feed

    printf("Username: %s ; pswd: %s\n", username, pswd);

    if(auth(username, pswd, cred_file)){
        logged = 1;
        err = write(info->sock, "230", 3);  // 230 means user logged in
        error(err, -1, "Sending failed.\n");
    } else {      // Error authenticating
        err = write(info->sock, "530", 3);  // 530 means not logged in
        error(err, -1, "Sending failed.\n");
        fclose(cred_file);
        pthread_exit(&retval);      // Close thread and wait for new connection
    }
    fclose(cred_file);

    // Changing client directory to base_dir
    chdir(info->base_dir);
    printf("Current dir: %s\n", getcwd(buf, sizeof(buf)));

    while(logged){
        // Waiting command from client
        bzero(buf, sizeof(buf));
        err = read(info->sock, buf, sizeof(buf));
        error(err, -1, "Reading failed.\n");

        if(buf[0] == '\0')         // Means client killed program
            pthread_exit(&retval);  // Close thread and wait for new connection

        // Remove line feed if needed
        if(iscntrl(buf[strlen(buf)-1]))
            buf[strlen(buf)-1] = '\0';

        printf("Received: %s\n", buf);

        // Split command string
        bzero(cmd_name, sizeof(cmd_name));
        strcpy(cmd_name, buf);
        ptr = strtok(cmd_name, delim);
        printf("cmd_name: %s\n", cmd_name);

        if(!strcmp(cmd_name, "pwd")){
            printf("Command sent from client: %s\n", buf);
            if(strcmp(cmd_name, buf)){
                err = write(info->sock, "Invalid command. Did you mean 'pwd'?", 36);
                error(err, -1, "Sending failed.\n");
            } else{
                getcwd(buf, sizeof(buf));
                err = write(info->sock, buf, strlen(buf));
                error(err, -1, "Sending failed.\n");
            }
        } else if(!strcmp(cmd_name, "ls")){
            printf("Command sent from client: %s\n", buf);
            err = write(info->sock, "You said ls", 11);
            error(err, -1, "Sending failed.\n");
        } else if(!strcmp(cmd_name, "close")){
            printf("Command sent from client: %s. Client disconnected.\n", buf);
            err = write(info->sock, "530", 3);
            error(err, -1, "Sending failed.\n");
            logged = 0;
        } else {
            printf("Command sent from client: %s\n", buf);
            err = write(info->sock, "Not implemented yet", 19);
            error(err, -1, "Sending failed.\n");
        }
    }

    free(info);
    pthread_exit(&retval);  // Close thread and wait for new connection
}

int main(int argc,char *argv[]){
    struct sockaddr_in server, client;
    int sock1, sock2;       // file descriptors
    unsigned short port = 2121;     // we will use port 2121
    pthread_t tid;
    int err, len;
    client_info * info;
    char base_dir[50], original_dir[50];
    FILE * dir_file;

    /* Store directory that contains ftp_server program and the files */
    getcwd(original_dir, sizeof(original_dir));

    /* Read file to get base directory */
    dir_file = fopen("base_dir.txt", "r");
    fgets(base_dir, 50, dir_file);
    // Remove line feed if needed
    if(iscntrl(base_dir[strlen(base_dir)-1]))
        base_dir[strlen(base_dir)-1] = '\0';
    printf("Base_dir: %s\n", base_dir);
    fclose(dir_file);

    /* Socket creation:
        int socket(int domain, int type, int protocol);
        domain: AF_INET is used for IPv4 Internet protocols
        type:   SOCK_STREAM provides sequenced, reliable, two-way,
                connection-based byte streams (TCP). An out-of-band data
                transmission mechanism may be supported
        protocol: Normally only a single protocol exists to support a
                particular socket type within a given protocol family,
                in which case protocol can be specified as 0.
    */
    printf("Creating socket\n");
    sock1 = socket(AF_INET, SOCK_STREAM, 0);
    error(sock1, -1, "Socket creation failed.\n");

    // Now the socket exists in a name space (address family) but has no
    // address assigned to it. Let's assign a name to the socket.
    memset(&server, '\0', sizeof(server));          // zero structure out
    server.sin_family = AF_INET;                    // match socket call
    server.sin_port = htons(port);                  // bind to port
    server.sin_addr.s_addr = htonl(INADDR_ANY);     // bind to all interfaces

    /* Binding:
        int bind(int sockfd, const struct sockaddr *addr, socklen_t addrlen);
        sockfd: socket's file descriptor;
        addr: address to be assigned to the socket (in this case, server).
                Server is a sockaddr_in struct so we need to cast
                (struct sockaddr_in *) to (struct sockaddr *);
        addrlen: size of structure pointed to by addr.
    */
    printf("Binding\n");
    err = bind(sock1, (struct sockaddr*)&server, sizeof(server));
    error(err, -1, "Binding failed.\n");

    /* This loop will keep listing and accepting connections with new clients
    and creating new threads */
    while(1){
        /* Now server can listen.
        int listen(int sockfd, int backlog);
        listen() marks the socket referred to by sockfd as a passive
            socket, that is, as a socket that will be used to accept incoming
            connection requests using accept(2).
        sockfd: socket's file descriptor;
        backlog: defines the maximum length to which the queue of pending
                connections for sockfd may grow.
        */
        printf("Listening\n");
        err = listen(sock1, 3);               // TODO: How big should be backlog?
        error(err, -1, "Listening failed.\n");

        /* Now server needs to explicitly accept connections.
        int accept(int sockfd, struct sockaddr *addr,
            socklen_t *addrlen);
        Extracts the first connection on the queue of pending connections,
            creates a new socket with the same socket type protocol and
            address family as the specified socket, and allocates a new
            file descriptor for that socket.
        sockfd: socket's file descriptor;
        address: pointer to store client address.
            (struct sockaddr_in *) cast to (struct sockaddr *);
        addrlen: pointer to store the returned size of addr.
        */
        printf("Accepting\n");
        memset(&client, '\0', sizeof(client));      // zero structure out
        len = sizeof(client);
        sock2 = accept(sock1, (struct sockaddr*)&client, &len);
        error(sock2, -1, "Accepting failed.\n");

        // For each new connection stablished, create a new thread to handle it
        info = malloc(sizeof *info);
        info->sock = sock2;
        strcpy(info->base_dir, base_dir);
        strcpy(info->original_dir, original_dir);
        if(pthread_create(&tid, NULL, &handle_client, (void *) info)){
            free(info);
            printf("ERROR: thread creation failed.\n");
            exit(1);
        }
    }

    // i = 1;
    // while(1){
    //     memset(buf, '\0', strlen(buf)*sizeof(char));
    //     recv(sock2, buf, 99, 0);
    //     // err = read(sock2, buf, strlen(buf)*sizeof(char));
    //     if(err < 0){
    //         printf("Reading failed\n");
    //         exit(1);
    //     }
    //     for(int j = 0 ; j < 100 ; j++){
    //         printf("%d: %c\t", buf[j], buf[j]);
    //     }
    //     printf("received buf: %s\n", buf);
    //     sscanf(buf, "%s%s", command, param);
    //     printf("Command received: %s\n", command);
    //     printf("Param received: %s\n", param);
    //
    //     if(!strcmp(command, "pwd")){
    //         // sscanf(buf, "%s", param);
    //
    //         system("pwd>temp.txt");
    //         i = 0;
    //         FILE*f = fopen("temp.txt","r");
    //         while(!feof(f))
    //             buf[i++] = fgetc(f);
    //         buf[i-1] = '\0';
    //         fclose(f);
    //         send(sock2, buf, 100, 0);
    //     }
    //     // else if(){
    //     //
    //     // }
    //     else printf("Not implemented yet\n");
    //
    //     scanf("%*[^\n]");
    //     scanf("%*c");
    // }


    // i = 1;
    // while(1){
    //     recv(sock2, buf, 100, 0);
    //     sscanf(buf, "%s", command);
    //     if(!strcmp(command, "ls")){
    //         system("ls >temps.txt");
    //         i = 0;
    //         stat("temps.txt",&obj);
    //         size = obj.st_size;
    //         send(sock2, &size, sizeof(int),0);
    //         filehandle = open("temps.txt", O_RDONLY);
    //         sendfile(sock2,filehandle,NULL,size);
    //     }
    //     else if(!strcmp(command,"get")){
    //         sscanf(buf, "%s%s", filename, filename);
    //         stat(filename, &obj);
    //         filehandle = open(filename, O_RDONLY);
    //         size = obj.st_size;
    //         if(filehandle == -1)
    //             size = 0;
    //         send(sock2, &size, sizeof(int), 0);
    //         if(size)
    //             sendfile(sock2, filehandle, NULL, size);
    //
    //     }
    //     else if(!strcmp(command, "put")){
    //         int c = 0, len;
    //         char *f;
    //         sscanf(buf+strlen(command), "%s", filename);
    //         recv(sock2, &size, sizeof(int), 0);
    //         i = 1;
    //         while(1){
    //             filehandle = open(filename, O_CREAT | O_EXCL | O_WRONLY, 0666);
    //             if(filehandle == -1){
    //                 sprintf(filename + strlen(filename), "%d", i);
    //             }
    //             else break;
    //         }
    //         f = malloc(size);
    //         recv(sock2, f, size, 0);
    //         c = write(filehandle, f, size);
    //         close(filehandle);
    //         send(sock2, &c, sizeof(int), 0);
    //     }
    //     else if(!strcmp(command, "pwd")){
    //         system("pwd>temp.txt");
    //         i = 0;
    //         FILE*f = fopen("temp.txt","r");
    //         while(!feof(f))
    //             buf[i++] = fgetc(f);
    //         buf[i-1] = '\0';
    //         fclose(f);
    //         send(sock2, buf, 100, 0);
    //     }
    //     else if(!strcmp(command, "cd")){
    //         if(chdir(buf+3) == 0)
    //             c = 1;
    //         else c = 0;
    //         send(sock2, &c, sizeof(int), 0);
    //     }
    //     else if(!strcmp(command, "bye") || !strcmp(command, "quit")){
    //         printf("FTP server quitting..\n");
    //         i = 1;
    //         send(sock2, &i, sizeof(int), 0);
    //         exit(0);
    //     }
    // }
    return 0;
}
