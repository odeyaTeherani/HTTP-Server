#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <ctype.h>
#include <time.h>
#include <sys/stat.h>
#include <dirent.h>
#include "threadpool.h"
#include <pthread.h>
#define RFC1123FMT "%a, %d %b %Y %H:%M:%S GMT"
#define MAX_LEN 4000
#define VERSION "HTTP/1.1 "
#define SERV "\r\nServer: webserver/1.1\r\nDate: "
#define LOCATION "\r\nLocation: "
#define CONTENT "\r\nContent-Type: text/html\r\nContent-Length: "
#define CLOSE "\r\nConnection: close\r\n\r\n"
#define TITLE "<HTML><HEAD><TITLE>"
#define BODY "</TITLE></HEAD>\r\n<BODY><H4>"
#define H4 "</H4>\r\n"
#define ENDBODY "\r\n</BODY></HTML>\r\n"
#define ERR400 "400 Bad Request"
#define NOTE400 "Bad Request."
#define CON400 "113"
#define ERR404 "404 Not Found"
#define NOTE404 "File not found."
#define CON404 "112"
#define ERR500 "500 Internal Server Error"
#define NOTE500 "Some server side error."
#define CON500 "144"
#define ERR501 "501 Not supported"
#define NOTE501 "Method is not supported."
#define CON501 "129"
#define ERR403 "403 Forbidden"
#define NOTE403 "Access denied."
#define CON403 "111"
#define ERR302 "302 Found"
#define NOTE302 "Directories must end with a slash."
#define CON302 "123"
#define OKR "200 OK"
#define FCONT "\r\nContent-Length: "
#define LMODIF "\r\nLast-Modified: "
#define FTEN "\r\nContent-Type: "
#define HTSTART "<HTML>\r\n<HEAD><TITLE>Index of "
#define HTITLE "</TITLE></HEAD>\r\n\r\n<BODY>\r\n<H4>Index of "
#define CRTAB "</H4>\r\n\r\n<table CELLSPACING=8>\r\n<tr><th>Name</th><th>Last Modified</th><th>Size</th></tr>\r\n"
#define FINDIR "\r\n\r\n</table>\r\n<HR>\r\n<ADDRESS>webserver/1.0</ADDRESS>\r\n</BODY></HTML>\r\n"
#define POINTSLASH "./"
//DECLERATION OF FUNCTIONS 
int indexOf(char c, char* str, int start);
void copySubString(char* dst, char* src, int start, int end);
int createResponse(void* arg);
void badResponse(int* newsockfd, char* timebuf, char* path,char* name, char* note, char* cont);
void dirResponde(int* newsockfd, int filength, char* timebuf, char* path, char* dirname, char* modified);
void fileResponse(int* newsockfd, int filength, char* timebuf, char* path, char* modif);
char* okResponse(int* newsockfd, char* timebuf, char* path, unsigned long length,char* modified);
unsigned long numOfDigit(int x);
void load_file(int* newsockfd, char* path, char* timebuf);
int noPermissions(char *path);
void printDir(char* path);
char *get_mime_type(char *name);
int isNumber(char* str);
void badUsage();
void errorr(char* err);
void destAndNull (void* x);
/*****MAIN****/
int main(int argc, char *argv[]){
	if (argc !=4 ||!isNumber(argv[1])|| !isNumber(argv[2]) || !isNumber(argv[3]))
		badUsage();
	int sockfd, newsockfd[atoi(argv[3])];
	struct sockaddr_in serv_addr;
	struct sockaddr cli_addr;
	int client = sizeof(serv_addr); 
	if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0 ) //create an endpoint for communication
		errorr("ERROR opening socket\n");
	serv_addr.sin_family = AF_INET; /*initialize struct socaddr_in*/
	serv_addr.sin_addr.s_addr =INADDR_ANY;
	serv_addr.sin_port = htons(atoi(argv[1]));
	if(bind(sockfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0)
		errorr("ERROR on binding\n");//binding a name to a socket
	if ( listen(sockfd,5) < 0 ) // listen for connection of one client on the socket
		errorr("listen\n");
	threadpool* pool = create_threadpool(atoi(argv[2]));
	if (pool == NULL)
		badUsage();
	for(int i=0; i<atoi(argv[3]); i++){
		if((*(newsockfd+i) = accept(sockfd, &cli_addr, (socklen_t *)&client)) < 0){ //accept
			errorr("ERROR on accept\n");
		}
		dispatch(pool, createResponse, newsockfd+i ); //check input, write a response and closing socket
	}
	destroy_threadpool(pool);
	if (close(sockfd)<0)
		errorr("close");
	return 0;
}
/*Create Response- check input, write a response and closing socket */
int createResponse(void* arg){
	int* newsockfd = (int*) arg;
	char input [MAX_LEN]="";
	char timebuf[128];
	char modifbuff[128];
	time_t now;
	now = time(NULL);
	strftime(timebuf, sizeof(timebuf), RFC1123FMT, gmtime(&now));
	if ((read(*newsockfd,input, MAX_LEN)) < 0){
		badResponse(newsockfd,timebuf, NULL, ERR500, NOTE500, CON500);
		errorr("read\n");
	}
	struct stat stbuf;
	unsigned long sizef;
	int endLine=0, endFirst=0, endSecond=0;
	endLine = indexOf('\r', input, 0); // index of end line
	char firstLine[endLine];
	copySubString(firstLine, input, -1, endLine); //firstLine contain the firs line of the request input
	if ((endFirst = indexOf(' ', input,0)) == -1){
		badResponse(newsockfd,timebuf, NULL, ERR400, NOTE400, CON400);
		 return 0;
	}
	char get [endFirst]; //get contain first word of the input request
	if ((endSecond = indexOf(' ', input,endFirst+1)) == -1){
		badResponse(newsockfd,timebuf, NULL, ERR400, NOTE400, CON400);
		 return 0;
	}
	char secondWord [endSecond-endFirst]; //secondWord contain second word word of the input request
	if (endLine<=endSecond){
		badResponse(newsockfd,timebuf, NULL, ERR400, NOTE400, CON400);
		 return 0;
	}
	char thirdWord [endLine-endSecond];
	sscanf( firstLine, "%s %s %s", get, secondWord, thirdWord);
	if (strcmp(get,"GET")!=0){// first word is no GET
		badResponse(newsockfd,timebuf, NULL, ERR501, NOTE501, CON501);
		 return 0;
	}
	if (strcmp(thirdWord,"HTTP/1.0") != 0 && strcmp(thirdWord,"HTTP/1.1") != 0){// check version
		badResponse(newsockfd,timebuf, NULL, ERR400, NOTE400, CON400);
		 return 0;
	}
	if (strcmp(secondWord,"/")==0){ //empty path
		if( stat("index.html", &stbuf) == 0){ //index.html in home directory
			if (noPermissions("index.html")){
				badResponse(newsockfd, timebuf, "index.html", ERR403, NOTE403, CON403);
				return 0;
			}	
			sizef = stbuf.st_size;
			strftime(modifbuff, sizeof(timebuf), RFC1123FMT, gmtime(&stbuf.st_mtime));
			fileResponse(newsockfd, sizef, timebuf, "index.html",modifbuff);
			return 0;
		}		
		else{ // home directory
			stat(POINTSLASH, &stbuf);
			sizef = stbuf.st_size;
			strftime(modifbuff, sizeof(timebuf), RFC1123FMT, gmtime(&stbuf.st_mtime));
			dirResponde(newsockfd,sizef, timebuf, POINTSLASH,"/", modifbuff);
			return 0;
		}
	}
	if (secondWord[0]!= '/'||strlen(secondWord) ==1 ){ //Invalid path
		badResponse(newsockfd, timebuf, NULL, ERR404, NOTE404, CON404);
		return 0;
	}
	char path [strlen(secondWord)-1];
	copySubString(path,secondWord,0,strlen(secondWord));
	if(( stat(path, &stbuf ) != 0)){ //path request does not exists
		badResponse(newsockfd, timebuf, path, ERR404, NOTE404, CON404);
		 return 0;
	}
	else if((stbuf.st_mode & S_IFDIR)){// path is directory
		if(path[strlen(path)-1]!='/'){ // directory dont end with '/'
		badResponse(newsockfd, timebuf, path, ERR302, NOTE302, CON302);
		 return 0;
		}
		else{
			char npath[strlen(path)+11];
			strcpy(npath,path);
			strcat(npath,"index.html"); //add index.html to the end of path
			npath[strlen(npath)]='\0';
			if(( stat( npath, &stbuf ) != 0)){ // dir
				char dire [strlen(path)+2];
				strcpy(dire, POINTSLASH);
				strcat(dire, path);
				stat(dire, &stbuf);
				sizef = stbuf.st_size;
				strftime(modifbuff, sizeof(timebuf), RFC1123FMT, gmtime(&stbuf.st_mtime));
			if (noPermissions(dire)){ //check if the folder does not have permissions
				badResponse(newsockfd, timebuf, dire, ERR403, NOTE403, CON403);
				return 0;
			}
				dirResponde(newsockfd,sizef, timebuf, dire, path,modifbuff);// dir response
				 return 0;
			}
			else{ // dir/index.html
			stat(npath, &stbuf);
			sizef = stbuf.st_size;
			strftime(modifbuff, sizeof(timebuf), RFC1123FMT, gmtime(&stbuf.st_mtime));
			if (noPermissions(npath)){ // check permissions in file path
				badResponse(newsockfd, timebuf, npath, ERR403, NOTE403, CON403);
				return 0;
			}
			fileResponse(newsockfd, sizef, timebuf, npath,modifbuff);// file response
			return 0;
			}
		}
	}
	else if(S_ISREG( stbuf.st_mode ) == 0 || (stbuf.st_mode & S_IRUSR) == 0 ){// check 'read' permissions
		badResponse(newsockfd, timebuf, path, ERR403, NOTE403, CON403);
		 return 0;
	}
	else{ //regular file with permissions
		stat(path, &stbuf);
		sizef = stbuf.st_size;
		strftime(modifbuff, sizeof(timebuf), RFC1123FMT, gmtime(&stbuf.st_mtime));
		if (noPermissions(path)){
			badResponse(newsockfd, timebuf, path, ERR403, NOTE403, CON403);
			return 0;
		}
		fileResponse(newsockfd, sizef, timebuf, path,modifbuff); // file response
		return 0;
	}
	return 0;
}
/*Bad Response- Create an error message depending on the problem, and write to the socket */
void badResponse(int* newsockfd, char* timebuf, char* path ,char* name, char* note, char* cont){
	char* resp;
	int length; //clac the lenth of response
	length = strlen(VERSION)+strlen(name)+strlen(timebuf)+strlen(CONTENT)+strlen(cont)+strlen(CLOSE)
	+strlen(TITLE)+strlen(name)+strlen(BODY)+strlen(name)+strlen(H4)+strlen(note)+strlen(ENDBODY)+strlen(SERV);
	if (strcmp(name, ERR302) == 0)
		length+=strlen(LOCATION)+strlen(path)+1;
	resp = (char*)malloc(sizeof(char)*length+1); //allocate memory
	if (resp == NULL)
		return;
	strcpy(resp, VERSION); //Chaining message
	strcat(resp, name);
	strcat(resp, SERV);
	strcat(resp, timebuf);
	if (strcmp(name, ERR302) == 0){
		strcat(resp,LOCATION);
		strcat(resp, path);
		strcat(resp, "/");
	}
	strcat(resp, CONTENT);
	strcat(resp, cont);
	strcat(resp, CLOSE);
	strcat(resp, TITLE);
	strcat(resp, name);
	strcat(resp, BODY);
	strcat(resp, name);
	strcat(resp, H4);
	strcat(resp, note);
	strcat(resp, ENDBODY);
	resp[strlen(resp)] = '\0';
	if ( (write(*newsockfd,resp,strlen(resp))) < 0 )
		return;
	if (resp!=NULL)
		destAndNull(resp);
	if (close(*newsockfd) < 0 )
		return;
}
/*File Response- Create a response to a file request, and write it to a socket */
void fileResponse(int* newsockfd, int filength, char* timebuf, char* path,char *modified){
	char* resp = okResponse(newsockfd, timebuf, path, filength,modified); //header for a valid request
	if ( (write(*newsockfd,resp,strlen(resp))) < 0 )
		badResponse(newsockfd,timebuf, NULL, ERR500, NOTE500, CON500);
	load_file(newsockfd, path, timebuf);
	if (resp!=NULL)
		destAndNull(resp);
	if (close(*newsockfd) < 0 )
		badResponse(newsockfd,timebuf, NULL, ERR500, NOTE500, CON500);
}
/*Directory Response- Create a response to directory request, and write its content to the socket*/
void dirResponde(int* newsockfd, int dirlength, char* timebuf, char* path, char* dirname,char *modified){
	char name[strlen(dirname)-1];
	copySubString(name, dirname, -1, strlen(dirname)-1);
	char* htmlResp; //will contain the html part of response
	int htmlength = strlen(HTSTART)+ strlen(dirname)+ strlen(HTITLE)+strlen(POINTSLASH)+strlen(name)+strlen(CRTAB);
	struct dirent **dirList;
   	int n = scandir(path, &dirList, NULL, alphasort);// Create an array with the contents of the folder
	int size = (n*500) +htmlength; //size of memory needed for the table
	htmlResp = (char*)malloc(sizeof(char)*size+1);
	if (htmlResp == NULL)
		badResponse(newsockfd,timebuf, NULL, ERR500, NOTE500, CON500);
	strcpy(htmlResp, HTSTART); //Chaining message
	strcat(htmlResp, dirname);
	strcat(htmlResp, HTITLE);
	strcat(htmlResp, POINTSLASH);
	strcat(htmlResp, name);
	strcat(htmlResp,CRTAB);
	htmlResp[strlen(htmlResp)] = '\0';
	struct stat stbuf;
	unsigned long sizef;
	char* bufa = "<tr>\r\n<td><A HREF=\"";
	char* bufb = "\">";
	char* bufc = "</A></td><td>";
	char* bufd = "</td>\r\n<td>";
	char* bufe= "</td>\r\n</tr>";
	int s = 0;
	while (s<n) { //Creates a table row for each entity in the folder
		strcat(htmlResp, bufa);
		strcat(htmlResp,dirList[s]->d_name);
		char fileInDir[strlen(path)+strlen(dirList[s]->d_name)];
		strcpy(fileInDir,path);
		strcat(fileInDir,dirList[s]->d_name);
		stat(fileInDir, &stbuf);
		sizef = stbuf.st_size;
		if ((stbuf.st_mode & S_IFDIR))
			strcat(htmlResp, "/");
		strcat(htmlResp,bufb);
		strcat(htmlResp,dirList[s]->d_name);
		strcat(htmlResp,bufc);
		char* m = ctime(&stbuf.st_mtime);
		strcat(htmlResp,m);
		strcat(htmlResp,bufd);
		if (!(stbuf.st_mode & S_IFDIR)){
			unsigned long size = numOfDigit(sizef);
			char filenstr[size+1];
			sprintf(filenstr, "%lu", sizef);
			strcat(htmlResp,filenstr);
		}
		strcat(htmlResp,bufe);
		destAndNull(dirList[s]);
		s++;
	}
	strcat(htmlResp,FINDIR);
	htmlResp[strlen(htmlResp)] = '\0';
	char* resp = okResponse(newsockfd, timebuf, path, strlen(htmlResp),modified);//header for a valid request
	int finalsize = strlen(htmlResp)+strlen(resp);
	char* finalresp = malloc(sizeof(char)*finalsize+1); //final message
	if (finalresp == NULL)
		badResponse(newsockfd,timebuf, NULL, ERR500, NOTE500, CON500);
	strcpy(finalresp,resp); //Chaining final message
	strcat(finalresp,htmlResp);
	if ( (write(*newsockfd,finalresp,strlen(finalresp))) < 0 )
		badResponse(newsockfd,timebuf, NULL, ERR500, NOTE500, CON500);
	destAndNull(resp);
	destAndNull(finalresp);
	destAndNull(htmlResp);
	destAndNull(dirList);
	if (close(*newsockfd) < 0 )
		badResponse(newsockfd,timebuf, NULL, ERR500, NOTE500, CON500);
}
/*OK Response- Create a header response of a valid request*/
char* okResponse(int* newsockfd, char* timebuf, char* path, unsigned long length, char* modified){
	char*resp;
	char* ext = get_mime_type(path);
	unsigned long size = numOfDigit(length);
	char filenstr[size+1];
	sprintf(filenstr, "%lu", length);
	filenstr[strlen(filenstr)] = '\0';
	int resplength =strlen(VERSION) + strlen(OKR)+ strlen(SERV)+strlen(timebuf)+strlen(FCONT)+strlen(LMODIF)+
	+strlen(CLOSE)+strlen(filenstr)+strlen(modified);
	if (ext != NULL)
		resplength += strlen(FTEN)+ strlen(ext);
	resp = (char*)malloc(sizeof(char)*resplength+1);
	if (resp == NULL)
		badResponse(newsockfd,timebuf, NULL, ERR500, NOTE500, CON500);
	strcpy(resp, VERSION); //Chaining message
	strcat(resp, OKR);
	strcat(resp, SERV);
	strcat(resp, timebuf);
	if (ext != NULL){ //contain file content
	strcat(resp, FTEN);
	strcat(resp, ext);
	}
	strcat(resp, FCONT);
	strcat(resp, filenstr);
	strcat(resp, LMODIF);
	strcat(resp, modified);
	strcat(resp, CLOSE);
	resp[strlen(resp)] = '\0';
	return resp;
}
/*Load File- load file and write it to socket*/
void load_file(int* newsockfd, char* path, char* timebuf){
	size_t size = 1024;
	unsigned char buffer[size];
	FILE * f = fopen (path, "rb");
	if (f==NULL)
		badResponse(newsockfd,timebuf, NULL, ERR500, NOTE500, CON500);
	fseek (f, 0, SEEK_SET);
	int nbytes;
 	while((nbytes = fread (buffer,1,size, f)) > 0)
         	write(*newsockfd,buffer,nbytes);
	 fclose(f);
}
/* find index of specific char in string - return  -1 if fail*/
int indexOf(char c, char* str, int start){
	int i = start;
	for (; i<strlen(str); i++)
		if (*(str+i)==c)
			return i;
	return -1;
}
/* copy substring from src to dst between the index start-end*/
void copySubString(char* dst, char* src, int start, int end){
		int i=start+1, j=0;
		for (; i<(end-start); i++, j++)
			dst[j]=src[i];
		dst[end-start-1] = '\0';
}
/*get_mime_type - return ext*/
char *get_mime_type(char *name) {
char *ext = strrchr(name, '.');
if (!ext) return NULL;
if (strcmp(ext, ".html") == 0 || strcmp(ext, ".htm") == 0) return "text/html";
if (strcmp(ext, ".jpg") == 0 || strcmp(ext, ".jpeg") == 0) return "image/jpeg";
if (strcmp(ext, ".gif") == 0) return "image/gif";
if (strcmp(ext, ".png") == 0) return "image/png";
if (strcmp(ext, ".css") == 0) return "text/css";
if (strcmp(ext, ".au") == 0) return "audio/basic";
if (strcmp(ext, ".wav") == 0) return "audio/wav";
if (strcmp(ext, ".avi") == 0) return "video/x-msvideo";
if (strcmp(ext, ".mpeg") == 0 || strcmp(ext, ".mpg") == 0) return "video/mpeg";
if (strcmp(ext, ".mp3") == 0) return "audio/mpeg";
return NULL;
}
/*count num of digits in number*/
unsigned long numOfDigit(int x){
	if (x==0)
		return 1;
	int i=0;
	for(; x!=0; i++, x/=10);
	return i;
}
/*check permissions- return 1 if there is no permissions in path. 0-otherwise*/
int noPermissions(char *path){
	char partOfPath[strlen(path)];
	char checkedslash[strlen(path)];
	char *slash;
	struct stat bufst;
	copySubString(partOfPath,path,-1,strlen(path));
	stat(partOfPath, &bufst);
	if(!(bufst.st_mode & S_IFDIR)){ //in case that is file	
		if(!(bufst.st_mode & S_IRUSR) || !(bufst.st_mode & S_IRGRP) || !(bufst.st_mode & S_IROTH))
			return 1;
	}
	slash = strtok(partOfPath, "/");
	if (slash==NULL)
		return 0;
	strcpy(checkedslash,slash);
	while( slash != NULL ){	
		stat(checkedslash, &bufst);
		if((bufst.st_mode & S_IFDIR)) //in case that is dir	
			if(!(bufst.st_mode & S_IXUSR) ||!(bufst.st_mode & S_IXOTH) ||!(bufst.st_mode & S_IXGRP))
				return 1;		
		else if(!(bufst.st_mode & S_IRUSR) ||!(bufst.st_mode & S_IROTH) ||!(bufst.st_mode & S_IRGRP))
			return 1;
		slash = strtok(NULL, "/");
		if(slash != NULL){
			strcat(checkedslash,"/");
			strcat(checkedslash,slash);
		}
	}
	return 0; 
}
/* Check if String is a number - return 1 with success, 0 if fail*/
int isNumber(char* str){
	int i=0;
	for (; i<strlen(str); i++)// check if str[i] is a number. only integer
		if ( *(str+i) > '9' || *(str+i) <'0'|| i==8)
			return 0;
	return 1;
}
/* show an error meesage and exit(EXIT_FAILURE) */
void badUsage(){
	fprintf(stderr,"Usage: server <port> <pool-size> <max-number-of-request>\n");
	exit(EXIT_FAILURE);
}
/* Error- show an error meesage and exit(EXIT_FAILURE) */
void errorr(char* err){
	perror(err);
	exit(EXIT_FAILURE);
}
/* Destroy And Null- Destroys and set as Null */
void destAndNull (void* x){
	free(x);
	x = NULL;
}
