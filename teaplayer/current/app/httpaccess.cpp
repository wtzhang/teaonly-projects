
#include "httpaccess.h"
#include "talk/base/socketstream.h"
#include "talk/base/asyncsocket.h"

HttpAccess::HttpAccess() {
    http_socket_ = NULL;
    thread = new talk_base::Thread();
    thread->Start();
}

HttpAccess::~HttpAccess() {
    Close();
    delete thread;
}

bool HttpAccess::Open() {
    thread->Post(this, MSG_ACCESS_OPEN);
    return true;
}

void HttpAccess::Close(){
    if( http_socket_) {
        http_socket_->SignalCloseEvent.disconnect(this); 
        http_socket_->SignalConnectEvent.disconnect(this);
        http_socket_->Close(); 
        delete http_socket_; 
        http_socket_ = NULL;
    }
    thread->Stop();
}

void HttpAccess::OnMessage(talk_base::Message *msg) {
    switch (msg->message_id ) {
        case MSG_ACCESS_OPEN:
            doConnect();
            break;
    }
}

void HttpAccess::doConnect() {
    std::string host = "192.168.0.106";
    unsigned int port = 8080;

    createSocket();
    talk_base::SocketAddress addr(host, port);
    if (http_socket_->Connect(addr) < 0) {
        if ( ! http_socket_->IsBlocking() ) {
            Close();    
            signalBeginOfStream(false); 
        }
    }
}


void HttpAccess::createSocket() {
    talk_base::Thread* pth = talk_base::Thread::Current();
    talk_base::AsyncSocket* socket =
        pth->socketserver()->CreateAsyncSocket(SOCK_STREAM);
    http_socket_ = socket; 
    ASSERT(http_socket_);
    http_socket_->SignalConnectEvent.connect(this, &HttpAccess::OnConnectEvent);
    http_socket_->SignalReadEvent.connect(this, &HttpAccess::OnReadEvent);
    http_socket_->SignalCloseEvent.connect(this,   &HttpAccess::OnCloseEvent);
}

void HttpAccess::httpQuery() {
    char http_header[1024];
    snprintf(http_header, 1024, "GET /live.flv HTTP/1.0\r\nUser-Agent: EzPlayer\r\n");
    http_socket_->Send((const unsigned char *)http_header, strlen(http_header));
}

void HttpAccess::OnConnectEvent(talk_base::AsyncSocket* socket) {

    ASSERT(socket == http_socket_);

    if ( http_socket_->GetState() == talk_base::Socket::CS_CONNECTED) {
        httpQuery();
        signalBeginOfStream(true);  
    } else {
        signalBeginOfStream(false);  
    }
} 

void HttpAccess::OnCloseEvent(talk_base::AsyncSocket* socket, int err) {
    ASSERT(socket == http_socket_);
    signalEndOfStream();
}

// maybe there is some writeback info.
void HttpAccess::OnReadEvent(talk_base::AsyncSocket* socket) {
    ASSERT(socket == http_socket_);

    static bool skipped_header = false;
    static unsigned char buffer[1024*32];
    static int last_length = 0;

    int ret = http_socket_->Recv(&buffer[last_length], sizeof(buffer));

    if (!skipped_header) {
        if ( ret > 0) {
            last_length += ret;
            for(int i = 0; i <  last_length -3; i++) {
                if ( (buffer[i] == '\r') && (buffer[i+1] == '\n') && (buffer[i+2] == '\r') && (buffer[i+3] == '\n') ) {
                    skipped_header = true;
                    if ( (last_length-i-4) > 0)
                        signalData((unsigned char *)&buffer[i+4], (unsigned int)(last_length - i - 4));
                    last_length = 0;
                    break;
                }
            }
        } 
    } else if ( ret > 0) {
        signalData((unsigned char *)buffer, (unsigned int)ret);
    }
}

