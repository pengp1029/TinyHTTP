// 网络通信需要的头文件、库文件
#include<winsock2.h>
#pragma comment(lib,"ws2_32.lib")

#include<iostream>
#include<string>

#include<cstring>
#include<sys/types.h>
#include<sys/stat.h>

#include<stdio.h>

#include<windows.h>
#define PRINTF(str) std::cout<<"["<<__func__<<" - "<<__LINE__<<"]"<<str<<std::endl;

char base_path[] = "index2.html";
char resource_path[] = "E:/c++/tinyhttpd/src/htdocs";
char badrequest_path[] = "400.html";
char notfound_path[] = "404.html";
char cannot_execute_path[] = "500.html";
char notimplement_path[] = "501.html";
char cgi_path[] = "color.cgi";
void error_die(const std::string& str){
    std::cerr<<str<<std::endl;
    exit(1);
}  

/*
    startup：实现网络初始化
    参数：端口地址   使用unsigned short类型：合法端口号为0~65535
    返回值： 套接字（服务器端的套接字）

    如果*port=0，则动态分配一个可用的端口，如果不为0则使用指定的端口号
*/
int startup(unsigned short *port){
    // 初始化套接字库
    WSADATA data;
    int ret = WSAStartup(
        MAKEWORD(1,1),// http 1.1
        &data); 
    if(ret){
        error_die("WSAStartup Failed");
    }

    // 创建套接字
    int server_socket = socket(
        PF_INET,// 套接字类型
        SOCK_STREAM,// 数据流
        IPPROTO_TCP // TCP协议
        );
    if(server_socket == -1){
        // 创建失败
        error_die("socket build failed");
    }

    // 设置端口可复用：允许在服务器重启后立即重新使用相同的地址
    int opt = 1;
    ret = setsockopt(server_socket, SOL_SOCKET,SO_REUSEADDR,(const char*)&opt,sizeof(opt));
    if(ret == -1){
        error_die("setsockopt failed");
    }

    // 配置服务器端的网络地址和端口
    struct sockaddr_in server_addr;
    memset(&server_addr,0,sizeof(server_addr));
    server_addr.sin_family = AF_INET; //网络地址类型 IPV4地址族
    server_addr.sin_port = htons(*port);
    server_addr.sin_addr.S_un.S_addr = htonl(INADDR_ANY);

    // 绑定套接字
    ret = bind(server_socket,(struct sockaddr*)&server_addr,sizeof(server_addr));
    if(ret < 0){
        error_die("bind failed");
    }

    // 动态分配端口
    int nameLen = sizeof(server_addr);
    if(*port == 0){
        // 获取套接字本地地址信息
        ret = getsockname(server_socket,
            (struct sockaddr*)&server_addr,
            &nameLen);
        if(ret < 0){
            error_die("getsockname failed");
        }
        *port = server_addr.sin_port;
    }

    // 创建监听队列
    ret = listen(server_socket,20);
    if(ret<0){
        error_die("listen failed");
    }
    return server_socket;

}

// 从指定的套接字读取数据，保存到buff，返回实际读到的字节
int get_line(SOCKET sock,char *buff,int size){
    char c = 0;
    int idx = 0;
    while(idx < size-1 && c != '\n'){
        int n = recv(sock,&c, 1, 0);
        if(n>0){
            if(c=='\r'){
                n = recv(sock,&c,1,MSG_PEEK);
                if(n>0 && c=='\n'){
                    recv(sock,&c,1,0);
                }
                else{
                    c = '\n';
                }
            }
            buff[idx++] = c;
        }
        else{
            c = '\n';
        }
    }
    buff[idx] = '\0';
    return idx;
}

void headers(SOCKET client,const char* type){
    // 发送响应包的头信息
    char buff[1024];
    strcpy(buff,"HTTP/1.1 200 OK\r\n");
    send(client,buff,strlen(buff),0);

    strcpy(buff,"Server: PengpHttpd/0.0\r\n");
    send(client,buff,strlen(buff),0);

    /*strcpy(buff,"Content-type:text/html\n");
    send(client,buff,strlen(buff),0);*/
    char buf[1024];
    sprintf(buf,"Content-Type: %s\r\n",type);
    send(client,buf,strlen(buf),0);

    strcpy(buff,"\r\n");
    send(client,buff,strlen(buff),0);
}

void cat(SOCKET client,FILE *resource){
    // 发送资源实际内容
    char buff[4096];
    int count = 0;
    while(1){
        int read_size = fread(buff,sizeof(char),sizeof(buff),resource);
        if(read_size<=0){
            break;
        }
        send(client,buff,read_size,0);
        count += read_size;
    }
    PRINTF("totally send " + std::to_string(count) + "bytes");
}

void unimplement(SOCKET client){
    // 向指定套接字发送一个请求方法不支持的错误提示页面
    char buff[1024];
    strcpy(buff,"HTTP/1.1 501 Method Not Implemented\r\n");
    send(client,buff,strlen(buff),0);

    strcpy(buff,"Server: PengpHttpd/0.0\r\n");
    send(client,buff,strlen(buff),0);

    strcpy(buff,"Content-type:text/html\n");
    send(client,buff,strlen(buff),0);

    strcpy(buff,"\r\n");
    send(client,buff,strlen(buff),0);
    

    // 501 网页内容
    sprintf(buff,"%s/%s",resource_path,notimplement_path);
    FILE *resource = fopen(buff,"r");
    cat(client,resource);
    fclose(resource);

}

void not_found(SOCKET client){
    // 404响应 网页不存在
    
    char buff[1024];
    strcpy(buff,"HTTP/1.1 404 NOT FOUND\r\n");
    send(client,buff,strlen(buff),0);

    strcpy(buff,"Server: PengpHttpd/0.0\r\n");
    send(client,buff,strlen(buff),0);

    strcpy(buff,"Content-type:text/html\n");
    send(client,buff,strlen(buff),0);

    strcpy(buff,"\r\n");
    send(client,buff,strlen(buff),0);
    

    // 404 网页内容
    sprintf(buff,"%s/%s",resource_path,notfound_path);
    FILE *resource = fopen(buff,"r");
    cat(client,resource);
    fclose(resource);
}

void bad_request(SOCKET client){
    char buff[1024];
    strcpy(buff,"HTTP/1.1 400 BAD REQUEST\r\n");
    send(client,buff,strlen(buff),0);

    strcpy(buff,"Server: PengpHttpd/0.0\r\n");
    send(client,buff,strlen(buff),0);

    strcpy(buff,"Content-type:text/html\n");
    send(client,buff,strlen(buff),0);

    strcpy(buff,"\r\n");
    send(client,buff,strlen(buff),0);

    sprintf(buff,"%s/%s",resource_path,badrequest_path);
    FILE *resource = fopen(buff,"r");
    cat(client,resource);
    fclose(resource);
}

void cannot_execute(SOCKET client){
    char buff[1024];
    strcpy(buff,"HTTP/1.1 500 Internal Server Error\r\n");
    send(client,buff,strlen(buff),0);

    strcpy(buff,"Server: PengpHttpd/0.0\r\n");
    send(client,buff,strlen(buff),0);

    strcpy(buff,"Content-type:text/html\n");
    send(client,buff,strlen(buff),0);

    strcpy(buff,"\r\n");
    send(client,buff,strlen(buff),0);

    sprintf(buff,"%s/%s",resource_path,cannot_execute_path);
    FILE *resource = fopen(buff,"r");
    cat(client,resource);
    fclose(resource);
}

const char*getHeadType(const char*fileName){
    const char*ret = "text/html";
    const char*p = strrchr(fileName,'.');
    if(!p) return ret;
    p++;
    if(!strcmp(p,"css")) ret = "text/css";
    else if(!strcmp(p,"jpg")) ret = "image/jpeg";
    else if(!strcmp(p,"png")) ret = "image/png";
    else if(!strcmp(p,"js")) ret = "application/x-javascript";

    return ret;
}
void server_file(SOCKET client,const char* fileName){
    // 发送资源内容
    char buff[1024];
    int read_size = 1;

    // 请求报文的剩余数据行读完
    while(read_size > 0 && strcmp(buff,"\n")){
        read_size = get_line(client,buff,sizeof(buff));
        //PRINTF(buff);
    }

    // 从硬盘读取资源
    FILE *resource = nullptr;
    sprintf(buff,"%s/%s",resource_path,base_path);
    if(strcmp(fileName,buff)==0){
        resource = fopen(fileName,"r");
    }
    else
        resource = fopen(fileName,"rb");
    if(resource == nullptr){
        PRINTF("resource not found!");
        not_found(client);
    }
    else{
        // 发送头
        headers(client,getHeadType(fileName));

        // 发送请求的资源信息
        cat(client,resource);

        PRINTF("resource sended!");
    }
    fclose(resource);
}

void execute_cgi(SOCKET client,char*path,const char*method,char*query){
    char buff[2048];
    int read_size = 1;
    int content_length = -1;// POST body长度
    if(stricmp(method,"GET")==0){
        // 如果是GET方法，则读完后面的内容
        while((read_size>0)&&strcmp("\n",buff))
            read_size = get_line(client,buff,sizeof(buff));
    }
    else{
        read_size = get_line(client,buff,sizeof(buff));
        // POST方法，获取body长度
        while((read_size>0)&&strcmp("\n",buff)){
            buff[15] = '\0';
            if(stricmp(buff,"Content-Length:")==0){
                content_length = atoi(&(buff[16]));
            }
            read_size = get_line(client,buff,sizeof(buff));
        }
        // 没有指示body大小
        if(content_length==-1){
            PRINTF("bad request!");
            bad_request(client);
            return ;
        }
    }
    
    //not_found(client);
    //return;
    headers(client,"text/html");
    //sprintf(buff,"HTTP/1.1 200 OK\r\n");
    //send(client,buff,strlen(buff),0);

    // 创建管道用于进程通信
    HANDLE cgiOutputRead, cgiOutputWrite;
    HANDLE cgiInputRead, cgiInputWrite;

    // 管道属性
    SECURITY_ATTRIBUTES sa;
    sa.nLength = sizeof(sa);
    sa.bInheritHandle = TRUE;
    sa.lpSecurityDescriptor = 0;

    BOOL bCreate = CreatePipe(&cgiOutputRead,&cgiOutputWrite,&sa,0);
    if (!bCreate) {
        PRINTF("cannot execute!");
        cannot_execute(client);
        return ;
    }

    bCreate = CreatePipe(&cgiInputRead, &cgiInputWrite, &sa, 0);
    if (!bCreate) {
        PRINTF("cannot execute!");
        cannot_execute(client);
        return ;
    }

    // 子进程的启动属性
    STARTUPINFO si = {0};
    si.cb = sizeof(si);
    si.hStdOutput = cgiOutputWrite;
    si.hStdInput = cgiInputRead;
    si.dwFlags = STARTF_USESHOWWINDOW | STARTF_USESTDHANDLES;

    PROCESS_INFORMATION pi = {0};

    // 环境变量
    char meth_env[255];
    char query_env[255];
    char length_env[255];
    sprintf(meth_env,"REQUEST_METHOD=%s",method);

    PRINTF(path);
    PRINTF(method);
    PRINTF(query);
    char all_env[1024];
    char *env = all_env;
    strcpy(env,meth_env);
    env+=strlen(env)+1;
    if(stricmp(method,"GET")==0){
        sprintf(query_env,"QUERY_STRING=%s",query);
        PRINTF(query_env);
        strcpy(env,query_env);
        env+=strlen(env)+1;
        *env=0;
        PRINTF(all_env);
        //const char*envVars[] = {meth_env,query};
        bCreate = CreateProcess(path, 0, 0, 0, true, 0, all_env, 0, &si, &pi);
        if (bCreate==false) {
            PRINTF("create process failed!");
            cannot_execute(client);
            return ;
        }
    }
    else{
        sprintf(length_env,"CONTENT_LENGTH=%d",content_length);
        PRINTF(length_env);
        strcpy(env,length_env);
        //const char*envVars[] = {meth_env,length_env};
        //sprintf(meth_env,"%s %s",path,length_env);
        PRINTF(all_env);
        bCreate = CreateProcess(path,0,0, 0, true, 0, all_env, 0, &si, &pi);
        if (bCreate==false) {
            PRINTF("create process failed!");
            cannot_execute(client);
            return ;
        }
    }



    //char cmdLine[2048];//= "your_cgi_script.exe"; // 替换为实际的CGI脚本路径
    //sprintf(cmdLine,"%s/%s",resource_path,cgi_path);



    // 如果是POST则读body内容并写到cgi中
    char c = 0;
    if(stricmp(method,"POST")==0){
        for(int i=0;i<content_length;i++){
            recv(client,&c,1,0);
            DWORD bytesWritten;
            bool bWrite = WriteFile(cgiInputWrite, &c, 1, &bytesWritten, NULL);
            if(bWrite==false){
                cannot_execute(client);
                return;
            }
        }
    }

    // 等待子进程结束
    WaitForSingleObject(pi.hProcess, INFINITE);

    // 从 cgiOutputRead 读取数据
    PRINTF("read data");
    char data[2048];
    DWORD bytesRead;
    //DWORD dwMark=STILL_ACTIVE;

	
    if (ReadFile(cgiOutputRead, data, sizeof(data), &bytesRead, NULL) && bytesRead > 0) {
        data[bytesRead] = '\0';
        PRINTF(data);
        PRINTF(strlen(data));
        PRINTF(bytesRead)
        send(client,data,bytesRead,0);
        //GetExitCodeProcess(pi.hProcess,&dwMark);
    }
    PRINTF("read done");

    CloseHandle(cgiInputWrite);
    CloseHandle(cgiOutputWrite);
    CloseHandle(cgiInputRead);
    CloseHandle(cgiOutputRead);

    //send(client,'\0',1,0);

    // 关闭子进程和相关管道句柄
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
    PRINTF("PROCESS CLOSED");

}

// 处理用户请求的函数
DWORD WINAPI accept_request(LPVOID arg){
    char buff[1024];
    SOCKET client = (SOCKET)arg;

    // 请求头解析
    int read_size = get_line(client,buff,sizeof(buff));
    PRINTF(buff); // GET /(*.html 访问路径，默认为index.html) HTTP/1.1

    // 请求方式
    char method[255];
    int idx = 0;
    int write = 0;
    while(!isspace(buff[idx]) && write<sizeof(method)-1 && idx<sizeof(buff)){
        method[write++] = buff[idx++];
    }
    method[write] = '\0';
    PRINTF(method);
    // 检查请求方法是否被服务器支持
    if(stricmp(method,"GET")&&stricmp(method,"POST")){
        unimplement(client);
        return 0;
    }

    // POST方法使用CGI
    int cgi = 0;
    if(stricmp(method,"POST")==0)
        cgi = 1;

    // 资源文件路径 *.html
    char url[255];
    write = 0;
    // 跳过前置空格
    while(isspace(buff[idx]) && idx<sizeof(buff)){
        idx++;
    }
    while(!isspace(buff[idx]) && write<sizeof(url)-1 && idx<sizeof(buff)){
        url[write++] = buff[idx++];
    }
    url[write] = '\0';
    PRINTF(url);

    //是否使用GET方法传递参数
    char *query = nullptr;
    if(stricmp(method,"GET")==0){
        query = url;
        while((*query!='?')&&(*query!='\0'))
            query++;
        if(*query=='?'){
            cgi = 1;
            *query='\0';
            query++;
        }
    }

    char path[512] = "";
    sprintf(path,"%s%s",resource_path,url);
    if(path[strlen(path)-1]=='/'){
        strcat(path,base_path);
    }
    PRINTF(path);
    // 判断path类型 根据不同的路径判断是否添加index.html
    struct stat status;
    int ret = stat(path,&status);
    if(ret==-1){
        // 请求资源不存在
        while(read_size > 0 && strcmp(buff,"\n")){
            read_size = get_line(client,buff,sizeof(buff));
        }
        PRINTF("resource not found!");
        not_found(client);
    }
    else{
        if((status.st_mode & S_IFMT) == S_IFDIR){
            strcat(path,"/");
            strcat(path,base_path);
            PRINTF(path);
        }
        if((status.st_mode&S_IXUSR)||
           (status.st_mode&S_IXGRP)||
           (status.st_mode&S_IXOTH))
            cgi = 1;

        if(!cgi)
            server_file(client,path);
        else
            execute_cgi(client,path,method,query);
    }

    closesocket(client);
    return 0;
}

int main(void){
    unsigned short port = 80;
    int server_sock = startup(&port);
    std::cout<<"httpd server start, listening to "<<port<<" port"<<std::endl;
    
    struct sockaddr_in client_addr;
    int client_addr_len = sizeof(client_addr);
    while(1){
        // 阻塞式等待浏览器发起访问 保存连接信息
        int client_sock = accept(server_sock,(struct sockaddr*)&client_addr,&client_addr_len);
        if(client_sock == -1){
            error_die("accept failed");
        }

        // 使用新的套接字对用户进行交互
        DWORD threadId = 0;
        HANDLE ret = CreateThread(0,0,accept_request,(void *)client_sock,0,&threadId);
        if(ret == nullptr){
            error_die("thread create failed!");
        }
    }
    
    closesocket(server_sock);
    return 0;
}