#include <iostream>
#include <cstring>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include "Messages.h"

int main()
{
    int sockfd;
    struct sockaddr_in servaddr, cliaddr;

    if((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0)
    {
        perror("Socket creation failed");
        return -1;
    }

    memset(&servaddr, 0, sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    servaddr.sin_addr.s_addr = INADDR_ANY;
    servaddr.sin_port = htons(1234);

    if (bind(sockfd, (const struct sockaddr *)&servaddr, sizeof(servaddr)) < 0)
    {
        perror("Bind failed");
        return -1;
    }

    std::cout << "Feed handler listening on port 1234..." << std::endl;

    char buffer[1024];
    socklen_t len = sizeof(cliaddr);

    while (true)
    {
        int n = recvfrom(sockfd, (char *)buffer, 1024, MSG_WAITALL, (struct sockaddr *)&cliaddr, &len);

        if (n > 0)
        {
            MsgType type = static_cast<MsgType>(buffer[0]);

            if(type == MsgType::AddOrder)
            {
                AddOrderMsg* msg = reinterpret_cast<AddOrderMsg*>(buffer);

                std::cout << "[UDP] ADD ID=" << msg->id 
                          << " Px=" << msg->price 
                          << " Qty=" << msg->quantity 
                          << " Side=" << msg->side << std::endl;
            }
            else if (type == MsgType::CancelOrder)
            {
                std::cout << "[UDP] CANCEL received" << std::endl;
            }
        }
    }

    close(sockfd);
    return 0;
}