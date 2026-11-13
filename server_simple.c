#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <netinet/in.h>
#include <arpa/inet.h>

int lenfd(FILE *fd) {
	int len;

	fseek(fd, 0, SEEK_END);
	len = ftell(fd);
	fseek(fd, 0, SEEK_SET);
	return len;
}

void error400(int newsockfd){
	char text[256] = "<html><body>400 Bad request. Awful, even...</body></html>";
	char notfound[128];
	sprintf(notfound, "HTTP/1.0 400 Bad Request\nContent-Type: text/html\nContent-Length: %d\n\n", strlen(text));
	write(newsockfd, notfound, strlen(notfound));
	write(newsockfd, text, strlen(text));
	return;
}

void error404(int newsockfd){
	FILE *fd;
	int fsize, readbytes;
	char text[128], buf[256];

	fd = fopen("./ROOT/404.html", "rb");
	fsize = lenfd(fd);

	sprintf(text, "HTTP/1.0 404 Not Found\nContent-Type: text/html\nContent-Length: %d\n\n", fsize);
	write(newsockfd, text, strlen(text));
	while(1) {
		readbytes = fread(buf, 1, 255, fd);
		if (!readbytes) break;
		write(newsockfd, buf, readbytes);
	}
	fclose(fd);
	return;
}

void error501(int newsockfd){
	char text[256] = "<html><body>501 Not Implemented (yet)</body></html>";
	char notfound[128];
	sprintf(notfound, "HTTP/1.0 501 Bad Request\nContent-Type: text/html\nContent-Length: %d\n\n", strlen(text));
	write(newsockfd, notfound, strlen(notfound));
	write(newsockfd, text, strlen(text));
	return;
}

void append_log(char *ipaddr, char *buffer, FILE *logfd) {
	char logipaddr[544];

	if(strstr(buffer, "/chat/log.html")!=NULL || strstr(buffer, "frame")!=NULL) return;
	sprintf(logipaddr, "<address>%s</address>\n", ipaddr);
	fwrite(logipaddr, 1, strlen(logipaddr), logfd);

	sprintf(logipaddr, "<message>%s</message>\n", buffer);
	fwrite(logipaddr, 1, strlen(logipaddr), logfd);
}

int check_fname(char *fname) {
        char state, c;
	char *ptr;

	state = 0;
	ptr = fname;
        while(*ptr && *ptr!='?') {
                c = *ptr;
                if ((state && c == '/') || c == '_') return 0;
                state = (c == '.');
		ptr++;
        }
	return 1;
}

void sendfile(int newsockfd, char *filename) {
	FILE *fd;
	char *extension;
	char header[128], buf[256];
	int fsize, readbytes;

	fd = fopen(filename, "rb");
	if (!fd) {
		error404(newsockfd);
		return;
	}

	extension = strrchr(filename, '.');
	if(!extension) {
		error404(newsockfd);
		fclose(fd);
		return;
	}

	fsize = lenfd(fd);
	if(!strcmp(extension, ".png"))
		sprintf(header, "HTTP/1.0 200 OK\nContent-Type: image/png\nContent-Length: %d\n\n", fsize);
	else if(!strcmp(extension, ".jpeg"))
		sprintf(header, "HTTP/1.0 200 OK\nContent-Type: image/jpeg\nContent-Length: %d\n\n", fsize);
	else if(!strcmp(extension, ".gif"))
		sprintf(header, "HTTP/1.0 200 OK\nContent-Type: image/gif\nContent-Length: %d\n\n", fsize);
	else if(!strcmp(extension, ".html"))
		sprintf(header, "HTTP/1.0 200 OK\nContent-Type: text/html\nContent-Length: %d\n\n", fsize);
        else if(!strcmp(extension, ".wml"))
                sprintf(header, "HTTP/1.0 200 OK\nContent-Type: text/vnd.wap.wml\nContent-Length: %d\n\n", fsize);
        else if(!strcmp(extension, ".WMV"))
                sprintf(header, "HTTP/1.0 200 OK\nContent-Type: video/x-ms-wmv\nContent-Length: %d\n\n", fsize);
        else if(!strcmp(extension, ".MPG"))
                sprintf(header, "HTTP/1.0 200 OK\nContent-Type: video/mpeg\nContent-Length: %d\n\n", fsize);
        else if(!strcmp(extension, ".OGV"))
                sprintf(header, "HTTP/1.0 200 OK\nContent-Type: video/ogg\nContent-Length: %d\n\n", fsize);
        else if(!strcmp(extension, ".AVI"))
                sprintf(header, "HTTP/1.0 200 OK\nContent-Type: video/jpeg\nContent-Length: %d\n\n", fsize);
	else {
		error404(newsockfd);
		fclose(fd);
		return;
	}

	write(newsockfd, header, strlen(header));
	while(1) {
		readbytes = fread(buf, 1, 255, fd);
		if (!readbytes) break;
		write(newsockfd, buf, readbytes);
	}
	fclose(fd);
	return;
}

void status302(int newsockfd){
	char notfound[256];
	sprintf(notfound, "HTTP/1.0 302 Found\nLocation: /chat/msgform.html\n\n");
	write(newsockfd, notfound, strlen(notfound));
	return;
}

/*
void replace_plus(char *msg) {
	char *ptr;

	ptr = msg;
	while(*ptr) {
		if(*ptr == '+') *ptr = ' ';
		ptr++;
	}
}

void truncate_str(char *value) {
	char *ptr = value;
	int c = 0;
	while(*ptr && c < 0xff) {
		ptr++;
		c++;
	}
	*ptr = 0;
	return;
}

void do_send(int newsockfd, char *params, unsigned long addr) {
	const int userid = ((addr&0xffff0000) >> 16) + (addr&0xffff);
	const char eq[2] = "=";
	const char amp[2] = "&";
	char *state1, *state2;
	char *param = strtok_r(params, amp, &state1);
	char msg[256];
	msg[0] = 0;
	int count=0;
	while(param!=NULL) {
		char *name = strtok_r(param, eq, &state2);
		char *value = strtok_r(NULL, eq, &state2);
		if(!strcmp(name, "msg") && value != NULL) {truncate_str(value); strcpy(msg, value); count=1;}
		param = strtok_r(NULL, amp, &state1);
	}
	replace_plus(msg);
	if(count) {
		char fullmsg[272];
		sprintf(fullmsg, "<b>user%d</b>: %s", userid, msg);
		int pid = fork();
		if (!pid) {
			char *arguments[] = {"./ROOT/chat/log.pl", fullmsg, NULL};
			execvp(arguments[0], arguments);
			perror("An error occured");
			exit(1);
		}
		waitpid(pid, NULL, 0);
	}
	//usleep(500000);
	status302(newsockfd);
	return;
}
void do_log(int newsockfd) {
	//usleep(500000);
	char opentags[] = "<html><head><meta http-equiv='refresh' content='2'/></head><body bgcolor='#000000'><font color='#00ffaa'><pre>";
	char closetags[] = "</pre></font></body></html>";
	FILE *chatfd = fopen("./ROOT/chat/log.txt", "rb");
	int fsize = lenfd(chatfd) + strlen(opentags) + strlen(closetags);
	char header[128];
	sprintf(header, "HTTP/1.0 200 OK\nContent-Type: text/html\nContent-Length: %d\n\n", fsize);
	write(newsockfd, header, strlen(header));
	
	write(newsockfd, opentags, strlen(opentags));
	char buf[256];
	while(1) {
		int readbytes = fread(buf, 1, 255, chatfd);
		if (!readbytes) break;
		write(newsockfd, buf, readbytes);
	}
	write(newsockfd, closetags, strlen(closetags));
	fclose(chatfd);
	return;
}
*/

void do_msgbox(int newsockfd, char *params) {
	const char eq[2] = "=";
	const char amp[2] = "&";
	char *state1, *state2;
	char *param = strtok_r(params, amp, &state1);
	char *arguments[5];
	char script[] = "./ROOT/imgedit/_result/generate.py";
	char deftitle[] = "Warning";
	char defmsg[] = "Please fill in both fields";
	int count=0, pid, fsize, readbytes;
	char *name, *value;
	char header[128], buf[256], outputfile[64];
	FILE *fd;

	sprintf(outputfile, "./msgbox/result%d.png", getpid());

	arguments[0] = script;
	arguments[1] = outputfile;
	while(param!=NULL) {
		name = strtok_r(param, eq, &state2);
		value = strtok_r(NULL, eq, &state2);
		if(!strcmp(name, "msg")) {arguments[3] = value; count++;}
		else if(!strcmp(name, "title")) {arguments[2] = value; count++;}
		param = strtok_r(NULL, amp, &state1);
	}
	if (count != 2) {
		//error400(newsockfd);
		//return;
		arguments[2] = deftitle;
		arguments[3] = defmsg;
	}
	arguments[4] = NULL;
	pid = fork(); 
	if (!pid) {
		execvp(arguments[0], arguments);
		perror("ERROR running script\n");
		exit(1);
	}
	waitpid(pid, NULL, 0);
	fd = fopen(arguments[1], "rb+");
	if (!fd) {
		error404(newsockfd);
		return;
	}
	fsize = lenfd(fd);
	sprintf(header, "HTTP/1.0 200 OK\nContent-Type: image/png\nContent-Length: %d\n\n", fsize);
	write(newsockfd, header, strlen(header));

	while(1) {
		readbytes = fread(buf, 1, 255, fd);
		if (!readbytes) break;
		write(newsockfd, buf, readbytes);
	}
	fclose(fd);
	remove(arguments[1]);
	return;
}

void handle_get(int newsockfd, char *fname, unsigned long addr) {
	char filename[256];
	char *params, *fnametok;

	if (!check_fname(fname)) {
		error404(newsockfd);
		return;
	}
	if (!strcmp(fname, "/")) sprintf(filename, "./ROOT/index.html");
	else if (!strcmp(fname, "/chat/")) sprintf(filename, "./ROOT/chat/index.html");
	else {
		fnametok = strtok(fname, "?");
		params = strtok(NULL, "?");
		sprintf(filename, "./ROOT%s", fnametok);
	}
	printf("%s\n", filename);
	if (!strcmp(filename, "./ROOT/imgedit/msgbox")) do_msgbox(newsockfd, params);
	//else if (!strcmp(filename, "./ROOT/chat/send")) do_send(newsockfd, params, addr);
	//else if (!strcmp(filename, "./ROOT/chat/log.html")) do_log(newsockfd);
	else sendfile(newsockfd, filename);
	return;
}

void handle_post(int newsockfd, char *fname) {
	error501(newsockfd);
	return;
}

void server(int port) {
	char buffer[1024], request[16], http[8], fname[768];
	int sockfd, newsockfd, readbuf, scanbuf, option, pid;
	struct sockaddr_in serv_addr, cli_addr;
	struct timeval timeout;
	socklen_t clilen;
	FILE *logfd;
	char *ipaddr;

	sockfd = socket(AF_INET, SOCK_STREAM, 0);
	
	bzero((char *) &serv_addr, sizeof(serv_addr));
	serv_addr.sin_family = AF_INET;
	serv_addr.sin_addr.s_addr = INADDR_ANY;
	//serv_addr.sin_addr.s_addr = inet_addr("127.0.0.1");
	serv_addr.sin_port = htons(port);
	
	option = 1;
	timeout.tv_sec = 12;
	timeout.tv_usec = 0;
	setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &option, sizeof(option));
	if (bind(sockfd, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) < 0) {
		perror("Error on binding"); 
		exit(1);
	}

	listen(sockfd, 5);
	
	logfd = fopen("log.txt", "ab");
	if (!logfd) {
		printf("Error opening log.");
		exit(1);
	}

	clilen = sizeof(cli_addr);
	while (newsockfd = accept(sockfd, (struct sockaddr *) &cli_addr, &clilen)) {
		setsockopt(newsockfd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
		setsockopt(newsockfd, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout));
		pid = fork();
		if (!pid) {
			close(sockfd);
			readbuf = read(newsockfd, buffer, 1023);
			if (readbuf < 0) {
				perror("ERROR in reading"); 
				exit(1);
			}

			buffer[readbuf] = 0;
			ipaddr = inet_ntoa(cli_addr.sin_addr);
			append_log(ipaddr, buffer, logfd);

			scanbuf = sscanf(buffer, "%15s %767s HTTP/%7s\n", request, fname, http);
			if (scanbuf < 3) {
				error400(newsockfd);
				close(newsockfd);
				exit(0);
			}
			if (!strcmp(request, "GET")) handle_get(newsockfd, fname, cli_addr.sin_addr.s_addr);
			else if (!strcmp(request, "POST")) handle_post(newsockfd, fname);
			else {
				error501(newsockfd);	
				close(newsockfd);
				exit(0);
			}
			shutdown(newsockfd, SHUT_WR);
			close(newsockfd);
			exit(0);
		}
		signal(SIGCHLD, SIG_IGN);
		close(newsockfd);
	}
	close(sockfd);
	fclose(logfd);
	return;
}

int main(int argc, char **argv) {
	server(atoi(argv[1]));
	return 0;
}
