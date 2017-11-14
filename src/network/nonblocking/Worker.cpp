#include "Worker.h"

#include <iostream>

#include <sys/epoll.h>
#include <sys/socket.h>
#include <sys/types.h>

#include "Utils.h"

#include <protocol/Parser.h>
#include <map>
#include <memory>

#include <afina/Executor.h>
#include <afina/execute/Command.h>

#define MAXEVENTS 64

using namespace std;

namespace Afina {
namespace Network {
namespace NonBlocking {

// See Worker.h
Worker::Worker(shared_ptr<Afina::Storage> ps) : ps(ps) {}

// See Worker.h
Worker::~Worker() {
    // TODO: implementation here
}

void* Worker::StartNewThread(void *args) {
    pair<Worker*, int>* pargs = (pair<Worker*, int>*)(args);
    Worker* worker = pargs->first;
    int server_socket = pargs->second;
    
    delete pargs;

    worker->OnRun(server_socket);
    
    return NULL;
}

void Worker::Clean(void* args) {
    cout << "network debug: " << __PRETTY_FUNCTION__ << endl;

    int* epfd = (int*) args;

    close(*epfd);
    delete epfd;
    
    return;
}

// See Worker.h
void Worker::Start(int server_socket) {
    cout << "network debug: " << __PRETTY_FUNCTION__ << endl;
    
    running.store(1);

    pair<Worker*, int>* args = new pair<Worker*, int>(this, server_socket);
    if (pthread_create(&thread, NULL, Worker::StartNewThread, args) != 0) {
        throw runtime_error("cannot create a thread");
    }
}

// See Worker.h
void Worker::Stop() {
    cout << "network debug: " << __PRETTY_FUNCTION__ << endl;
    running.store(0);
}

// See Worker.h
void Worker::Join() {
    cout << "network debug: " << __PRETTY_FUNCTION__ << endl;
    pthread_join(this->thread, NULL);
}

// See Worker.h
void Worker::OnRun(int server_socket) {
    cout << "network debug: " << __PRETTY_FUNCTION__ << endl;

    // TODO: implementation here
    // 1. Create epoll_context here
    // 2. Add server_socket to context
    // 3. Accept new connections, don't forget to call make_socket_nonblocking on
    //    the client socket descriptor
    // 4. Add connections to the local context
    // 5. Process connection events
    //
    // Do not forget to use EPOLLEXCLUSIVE flag when register socket
    // for events to avoid thundering herd type behavior.

    map<int, Protocol::Parser> parsers;
    map<int, std::unique_ptr<Execute::Command>> commands;
    map<int, size_t> parsed;
    map<int, uint32_t> body_sizes;
    map<int, string> buffers;

    int epfd = epoll_create(MAXEVENTS);
    if (epfd == -1) {
      throw std::runtime_error("cannot epoll_create");
    }

    epoll_event ev;
    epoll_event events[MAXEVENTS];
    ev.events = EPOLLEXCLUSIVE | EPOLLHUP | EPOLLIN | EPOLLERR;
    ev.data.fd = server_socket; 

    if (epoll_ctl(epfd, EPOLL_CTL_ADD, server_socket, &ev) == -1) {
      throw std::runtime_error("cannot epoll_ctl");
    }

    int* args = new int {server_socket};

    pthread_cleanup_push(Worker::Clean, args);

    while (running.load()) {
        int n = epoll_wait(epfd, events, MAXEVENTS, -1);

        if (n == -1) {
            throw std::runtime_error("cannot epoll_wait");
        }

        for (int i = 0; i < n; i++) {
            int ev_soc = events[i].data.fd;

            // error or hangup
            if (((events[i].events & EPOLLERR) == EPOLLERR) || ((events[i].events & EPOLLHUP) == EPOLLHUP))  {
                cerr << "Error with socket connection\n";

                epoll_ctl(epfd, EPOLL_CTL_DEL, ev_soc, NULL);
                close(ev_soc);
                
                parsed.erase(ev_soc);
                buffers.erase(ev_soc);
                parsers.erase(ev_soc);
                commands.erase(ev_soc);
                body_sizes.erase(ev_soc);

                continue;
            }

            // event with server
            if (events[i].data.fd == server_socket) {
                
                // client connected
                if ((events[i].events & EPOLLIN) == EPOLLIN) {
                    int socket = accept(server_socket, NULL, NULL);
                    make_socket_non_blocking(socket);

                    ev.events = EPOLLIN | EPOLLRDHUP;
                    ev.data.fd = socket;
                    
                    if (epoll_ctl(epfd, EPOLL_CTL_ADD, socket, &ev) == -1) {
                        throw std::runtime_error("cannot epoll_ctl");
                    }

                    parsers[socket] = Protocol::Parser();
                    parsed[socket] = 0;
                    buffers[socket] = "";

                    continue;
                }

                // shouldn't reach this point, cos error is already managed
                continue;
            }

            // event not with server
            if ((events[i].events & EPOLLIN) == EPOLLIN) {
                ssize_t s = 0;
                char buf[512];
                bool need_clean = false;

                // read all data
                while (1) {
                    s = read(ev_soc, buf, sizeof(buf));

                    if (s == -1) {
                        if (errno != EAGAIN) {
                            need_clean = true;
                        }

                        break;
                    } else if (s == 0) {
                        need_clean = true;
                        break;
                    }

                    buf[s] = '\0';
                    buffers[ev_soc] += string(buf);
                }

                if (need_clean) {
                    cerr << "Unknown error with reading data.\n";

                    epoll_ctl(epfd, EPOLL_CTL_DEL, ev_soc, NULL);
                    close(ev_soc);
                
                    parsed.erase(ev_soc);
                    buffers.erase(ev_soc);
                    parsers.erase(ev_soc);
                    commands.erase(ev_soc);
                    body_sizes.erase(ev_soc);

                    continue;
                }

                // execute commands
                while (1) {

                    if (buffers[ev_soc].length() < 3) {
                        break;
                    }

                    // command not built yet
                    if (body_sizes.find(ev_soc) == body_sizes.end()) {
                        try {
                            if (!parsers[ev_soc].Parse(buffers[ev_soc].data(), buffers[ev_soc].length(), parsed[ev_soc])) {
                                break;
                            } else {
                                cout << "Parsed: " << parsed[ev_soc] << endl;
                                cout << "Buffer: " << buffers[ev_soc] << endl;
                                buffers[ev_soc].erase(0, parsed[ev_soc]);
                                parsed[ev_soc] = 0;

                                uint32_t body_size;
                                auto command = parsers[ev_soc].Build(body_size);
                                commands[ev_soc] = move(command);
                                body_sizes[ev_soc] = body_size;
                            }
                        } catch(...) {
                            cerr << "Error: Command not built and cant parse input\n";
                            cerr << "Buffer: " << buffers[ev_soc] << endl;
                            cerr << "Length: " << buffers[ev_soc].length() << endl;

                            string ret = "ERROR\r\n";
                            send(ev_soc, ret.data(), ret.size(), 0);

                            epoll_ctl(epfd, EPOLL_CTL_DEL, ev_soc, NULL);
                            close(ev_soc);

                            parsers.erase(ev_soc);
                            parsed.erase(ev_soc);
                            commands.erase(ev_soc);
                            body_sizes.erase(ev_soc);
                            buffers.erase(ev_soc);

                            break;
                        }
                    }

                    // command already built
                    cout << "Left to parse: " << body_sizes[ev_soc] << endl;
                    if (buffers[ev_soc].length() < body_sizes[ev_soc]) {
                        break;
                    }

                    string ret;
                    string args;
                    copy(buffers[ev_soc].begin(), buffers[ev_soc].begin() + body_sizes[ev_soc], back_inserter(args));
                    buffers[ev_soc].erase(0, body_sizes[ev_soc]);
                    if (buffers[ev_soc].length() > 0 && (buffers[ev_soc][0] == '\r' || buffers[ev_soc][0] == '\n')) {
                        buffers[ev_soc].erase(buffers[ev_soc].begin());
                    }
                    if (buffers[ev_soc].length() > 0 && (buffers[ev_soc][0] == '\r' || buffers[ev_soc][0] == '\n')) {
                        buffers[ev_soc].erase(buffers[ev_soc].begin());
                    }

                    try {
                        commands[ev_soc]->Execute(*(this->ps), args, ret);
                    } catch(...) {
                        ret = "ERROR";
                    }

                    ret += "\r\n";

                    cout << "Return: " << ret << endl;

                    if (send(ev_soc, ret.data(), ret.size(), 0) <= 0) {
                      epoll_ctl(epfd, EPOLL_CTL_DEL, ev_soc, NULL);
                      close(ev_soc);

                      parsers.erase(ev_soc);
                      parsed.erase(ev_soc);
                      commands.erase(ev_soc);
                      body_sizes.erase(ev_soc);
                      buffers.erase(ev_soc);
                      break;
                    }

                    parsers[ev_soc].Reset();

                    body_sizes.erase(ev_soc);
                    commands.erase(ev_soc);
                }

                continue;
            }

            // shouldn't reach this point, cos error is already managed
            continue;
        }
    }

    pthread_cleanup_pop(1);
    pthread_exit(NULL);
}

} // namespace NonBlocking
} // namespace Network
} // namespace Afina
