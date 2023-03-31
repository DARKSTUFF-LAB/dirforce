#include <iostream>
#include <string>
#include <cstring>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <vector>
#include <thread>
#include <cstdlib>
#include <unistd.h>
#include <mutex>
#include <fstream>
#include <openssl/bio.h>
#include <openssl/ssl.h>
#include <openssl/err.h>
#include "arguments.cpp"
#include "robotxt.cpp"

using namespace std;

int wordlistcount;
ifstream wordlist;
int sslOption; //check if ssl is enabled
string host;
string address;
mutex wordlist_lock;
mutex progresslock;
int progressCounter = 0;
string percent;
string http_method;
int requestOption;
string readrequest;
string suffixe;
string preffixe;
string port;

void progress() {
    progressCounter++;
    percent = "\r\033[1;37m" + to_string((progressCounter * 100) / wordlistcount) + "% " + to_string(progressCounter);
    cout.flush();
    cout << percent;
    cout.flush();
    cout << "\r";
}

int lineCounter(std::string filename) {
    std::ifstream infile(filename);
    int line_count = 0;
    std::string line;
    while (std::getline(infile, line)) {
        line_count++;
    }
    return line_count;
}

int request(const struct sockaddr *dest_addr, socklen_t addrlen)
{
    

    //create and connect a socket
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock == -1) {
        std::cerr << "Failed to create socket\n";
        return 1;
    }

    if (connect(sock, dest_addr, addrlen) == -1) {
        std::cerr << "Error connecting to server\n";
        return -1;
    }

    if (sslOption == 1) {
        //initialize ssl library
        SSL_library_init();
        SSL_load_error_strings();
        //create a new SSL_CTX object and initializes it for a TLS client
        SSL_CTX* ssl_ctx = SSL_CTX_new(TLS_client_method());
        if (ssl_ctx == nullptr) {
            std::cerr << "Error creating SSL context\n";
            return -1;
        }
        //create a new SSL object that is associated with the previously created SSL_CTX object
        SSL* ssl = SSL_new(ssl_ctx);
        if (ssl == nullptr) {
            std::cerr << "Error creating SSL connection\n";
            return -1;
        }
        //associates the SSL object ssl with the file descriptor sockfd
        if (SSL_set_fd(ssl, sock) == 0) {
            std::cerr << "Error attaching SSL connection to socket\n";
            return -1;
        }
    
        if (SSL_connect(ssl) != 1) {
            std::cerr << "Error performing SSL handshake\n";
            return -1;
        }
        // Check if the wordlist file is open
        if (!wordlist.is_open()) {
            std::cerr << "Failed to open wordlist file\n";
            return 1;
        }

        string word;
        while(true) {
            wordlist_lock.lock();
            getline(wordlist, word);
            wordlist_lock.unlock();
            if (wordlist.eof()) {
                break;
            }
            string path = address + preffixe + word + suffixe;
            string rawrequest;
            if (requestOption == 0) {
                rawrequest = http_method + " " + path + " HTTP/1.1\r\nHost: " + host + "\r\n" + "Connection: keep-alive\r\nUser-Agent: Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/58.0.3029.110 Safari/537.3\r\n\r\n";
            } else {
                rawrequest = readrequest.substr(0, readrequest.find("PATH")) + path + readrequest.substr(readrequest.find("PATH") + 4);
                rawrequest = rawrequest.substr(0, rawrequest.find("HOSTNAME")) + host + rawrequest.substr(rawrequest.find("HOSTNAME") + 8);
            }
            int n = SSL_write(ssl, rawrequest.c_str(), rawrequest.size());
            if (n <= 0) {
                std::cerr << "Error sending HTTP request\n";
                return -1;
            }
     
            char buf[80000];
            int len;
            string response;
            while ((len = SSL_read(ssl, buf, sizeof(buf))) > 0) {
                response.append(buf, len);
                if (response.find("\r\n\r\n") != string::npos) break;

            }
            if (response.find("HTTP/") != string::npos) {
                if (response.find("HTTP/") != string::npos) {
                    string status = response.substr(response.find("HTTP/") + 9, 3);
                    if (stoi(status) != 404) {
                        int http_code = stoi(status);
                        string color, color2;
                        if (http_code < 200) {color = "\033[1;34m"; /* yellow */}
                        else if (http_code < 300) {color = "\033[1;32m";color2 = "\033[0;32m"; /* green */}
                        else if (http_code < 400) {color = "\033[1;33m";color2 = "\033[0;33m"; /* blue */}
                        else if (http_code < 500) {color = "\033[1;35m";color2 = "\033[0;35m"; /* purple */}
                        else if (http_code < 600) {color = "\033[1;31m";color2 = "\033[0;31m"; /* red */}
                        string output = color + status + "   " + color2 + path + "\n";
                        cout.flush();
                        cout << output;
                    }
                }
                progresslock.lock();
                progress();
                progresslock.unlock();
            } else {//if the server close the connection create new socket connection
                request(dest_addr, addrlen);
                break;
            }
        }
    } else {
        string word;
        while(true) {
            wordlist_lock.lock();
            getline(wordlist, word);
            wordlist_lock.unlock();
            if (wordlist.eof()) {
                break;
            }
            string path = address + word;
            string rawrequest;
            if (requestOption == 0) {
                rawrequest = http_method + " " + path + " HTTP/1.1\r\nHost: " + host + "\r\n" + "Connection: keep-alive\r\nUser-Agent: Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/58.0.3029.110 Safari/537.3\r\n\r\n";
            } else {
                rawrequest = readrequest.substr(0, readrequest.find("PATH")) + path + readrequest.substr(readrequest.find("PATH") + 3);
            }
            // Send the HTTP request  without ssl
            if (send(sock, rawrequest.c_str(), rawrequest.size(), 0) == -1) {
                cout << "error \n";
                return 0;
            }

            // Receive the response to the request
            std::string response;
            char buf[4096];
            int num_bytes;
            while ((num_bytes = recv(sock, buf, sizeof(buf), 0)) > 0) {
                response.append(buf, num_bytes);
                if (response.find("\r\n\r\n") != string::npos) break;
            }
            if (response.find("HTTP/") != string::npos) {
                string status = response.substr(response.find("HTTP/") + 9, 3);
                if (stoi(status) != 404) {
                    int http_code = stoi(status);
                    string color, color2;
                    if (http_code < 200) {color = "\033[1;34m"; /* yellow */}
                    else if (http_code < 300) {color = "\033[1;32m";color2 = "\033[0;32m"; /* green */}
                    else if (http_code < 400) {color = "\033[1;33m";color2 = "\033[0;33m"; /* blue */}
                    else if (http_code < 500) {color = "\033[1;35m";color2 = "\033[0;35m"; /* purple */}
                    else if (http_code < 600) {color = "\033[1;31m";color2 = "\033[0;31m"; /* red */}
                    string output = color + status + "   " + color2 + path + "\n";
                    cout.flush();
                    cout << output;
                }
                progresslock.lock();
                progress();
                progresslock.unlock();
            } else { //if the server close the connection create new socket connection
                request(dest_addr, addrlen);
                break;
            }
        }
    }
    close(sock);
    return 0;
}
int main(int argc, char* argv[]) {
    int threadCounter;
    string wordlistfile;
    arguments(argc, argv, &host, &wordlistfile, &threadCounter, &sslOption, &address, &http_method, &requestOption, &readrequest, &suffixe, &preffixe);
    wordlist.open(wordlistfile);
    wordlistcount = lineCounter(wordlistfile);

    cout << "[38;2;12;230;140m [39m[38;2;13;233;136m [39m[38;2;15;235;132m [39m[38;2;17;237;128m [39m[38;2;20;239;124m [39m[38;2;22;241;119m [39m[38;2;24;243;115m█[39m[38;2;27;245;111m█[39m[38;2;29;246;107m [39m[38;2;32;248;103m█[39m[38;2;35;249;98m█[39m[38;2;38;250;94m [39m[38;2;41;251;90m [39m[38;2;44;252;86m [39m[38;2;47;253;82m [39m[38;2;51;253;78m [39m[38;2;54;254;74m [39m[38;2;58;254;71m [39m[38;2;61;254;67m [39m[38;2;65;254;63m [39m[38;2;69;254;60m [39m[38;2;72;254;56m█[39m[38;2;76;254;53m█[39m[38;2;80;253;49m█[39m[38;2;84;253;46m█                            v0.2"
         << endl << "[38;2;17;237;128m [39m[38;2;20;239;124m [39m[38;2;22;241;119m [39m[38;2;24;243;115m [39m[38;2;27;245;111m [39m[38;2;29;246;107m░[39m[38;2;32;248;103m█[39m[38;2;35;249;98m█[39m[38;2;38;250;94m░[39m[38;2;41;251;90m░[39m[38;2;44;252;86m [39m[38;2;47;253;82m [39m[38;2;51;253;78m [39m[38;2;54;254;74m [39m[38;2;58;254;71m [39m[38;2;61;254;67m [39m[38;2;65;254;63m [39m[38;2;69;254;60m [39m[38;2;72;254;56m [39m[38;2;76;254;53m [39m[38;2;80;253;49m░[39m[38;2;84;253;46m█[39m[38;2;88;252;43m█[39m[38;2;92;251;40m░[39m[38;2;96;250;37m [39m[38;2;100;249;34m [39m[38;2;104;247;31m [39m[38;2;109;246;28m [39m[38;2;113;244;26m [39m[38;2;117;242;23m [39m[38;2;121;240;21m [39m[38;2;125;239;19m [39m[38;2;130;236;16m [39m[38;2;134;234;14m [39m[38;2;138;232;13m [39m[38;2;142;229;11m [39m[38;2;147;227;9m [39m[38;2;151;224;8m [39m[38;2;155;221;6m [39m[38;2;159;218;5m [39m[38;2;163;215;4m [39m[38;2;167;212;3m [39m[38;2;171;209;2m [39m[38;2;175;206;2m [39m[38;2;179;202;1m [39m[38;2;183;199;1m [39m[38;2;187;195;1m [39m[38;2;190;192;1m [39m[38;2;194;188;1m [39m[38;2;197;184;1m [39m[38;2;201;180;1m [39m[38;2;204;177;2m [39m[38;2;208;173;2m [39m[38;2;211;169;3m [39m[38;2;214;165;4m [39m[38;2;217;161;5m [39m[38;2;220;156;6m [39m[38;2;223;152;7m[39m"
         << endl << "[38;2;24;243;115m [39m[38;2;27;245;111m [39m[38;2;29;246;107m [39m[38;2;32;248;103m [39m[38;2;35;249;98m [39m[38;2;38;250;94m░[39m[38;2;41;251;90m█[39m[38;2;44;252;86m█[39m[38;2;47;253;82m [39m[38;2;51;253;78m█[39m[38;2;54;254;74m█[39m[38;2;58;254;71m [39m[38;2;61;254;67m█[39m[38;2;65;254;63m█[39m[38;2;69;254;60m█[39m[38;2;72;254;56m█[39m[38;2;76;254;53m█[39m[38;2;80;253;49m█[39m[38;2;84;253;46m [39m[38;2;88;252;43m█[39m[38;2;92;251;40m█[39m[38;2;96;250;37m█[39m[38;2;100;249;34m█[39m[38;2;104;247;31m█[39m[38;2;109;246;28m█[39m[38;2;113;244;26m [39m[38;2;117;242;23m [39m[38;2;121;240;21m█[39m[38;2;125;239;19m█[39m[38;2;130;236;16m█[39m[38;2;134;234;14m█[39m[38;2;138;232;13m█[39m[38;2;142;229;11m█[39m[38;2;147;227;9m [39m[38;2;151;224;8m [39m[38;2;155;221;6m█[39m[38;2;159;218;5m█[39m[38;2;163;215;4m█[39m[38;2;167;212;3m█[39m[38;2;171;209;2m█[39m[38;2;175;206;2m█[39m[38;2;179;202;1m [39m[38;2;183;199;1m [39m[38;2;187;195;1m█[39m[38;2;190;192;1m█[39m[38;2;194;188;1m█[39m[38;2;197;184;1m█[39m[38;2;201;180;1m█[39m[38;2;204;177;2m [39m[38;2;208;173;2m [39m[38;2;211;169;3m [39m[38;2;214;165;4m█[39m[38;2;217;161;5m█[39m[38;2;220;156;6m█[39m[38;2;223;152;7m█[39m[38;2;226;148;9m█[39m[38;2;228;144;10m [39m[38;2;231;140;12m[39m"
         << endl << "[38;2;32;248;103m [39m[38;2;35;249;98m [39m[38;2;38;250;94m█[39m[38;2;41;251;90m█[39m[38;2;44;252;86m█[39m[38;2;47;253;82m█[39m[38;2;51;253;78m█[39m[38;2;54;254;74m█[39m[38;2;58;254;71m░[39m[38;2;61;254;67m█[39m[38;2;65;254;63m█[39m[38;2;69;254;60m░[39m[38;2;72;254;56m░[39m[38;2;76;254;53m█[39m[38;2;80;253;49m█[39m[38;2;84;253;46m░[39m[38;2;88;252;43m░[39m[38;2;92;251;40m█[39m[38;2;96;250;37m░[39m[38;2;100;249;34m░[39m[38;2;104;247;31m░[39m[38;2;109;246;28m█[39m[38;2;113;244;26m█[39m[38;2;117;242;23m░[39m[38;2;121;240;21m [39m[38;2;125;239;19m [39m[38;2;130;236;16m█[39m[38;2;134;234;14m█[39m[38;2;138;232;13m░[39m[38;2;142;229;11m░[39m[38;2;147;227;9m░[39m[38;2;151;224;8m░[39m[38;2;155;221;6m█[39m[38;2;159;218;5m█[39m[38;2;163;215;4m░[39m[38;2;167;212;3m░[39m[38;2;171;209;2m█[39m[38;2;175;206;2m█[39m[38;2;179;202;1m░[39m[38;2;183;199;1m░[39m[38;2;187;195;1m█[39m[38;2;190;192;1m [39m[38;2;194;188;1m█[39m[38;2;197;184;1m█[39m[38;2;201;180;1m░[39m[38;2;204;177;2m░[39m[38;2;208;173;2m░[39m[38;2;211;169;3m█[39m[38;2;214;165;4m█[39m[38;2;217;161;5m [39m[38;2;220;156;6m█[39m[38;2;223;152;7m█[39m[38;2;226;148;9m░[39m[38;2;228;144;10m░[39m[38;2;231;140;12m░[39m[38;2;233;136;14m█[39m[38;2;236;131;16m█[39m[38;2;238;127;18m[39m"
         << endl << "[38;2;41;251;90m [39m[38;2;44;252;86m█[39m[38;2;47;253;82m█[39m[38;2;51;253;78m░[39m[38;2;54;254;74m░[39m[38;2;58;254;71m░[39m[38;2;61;254;67m█[39m[38;2;65;254;63m█[39m[38;2;69;254;60m░[39m[38;2;72;254;56m█[39m[38;2;76;254;53m█[39m[38;2;80;253;49m [39m[38;2;84;253;46m░[39m[38;2;88;252;43m█[39m[38;2;92;251;40m█[39m[38;2;96;250;37m [39m[38;2;100;249;34m░[39m[38;2;104;247;31m [39m[38;2;109;246;28m [39m[38;2;113;244;26m [39m[38;2;117;242;23m░[39m[38;2;121;240;21m█[39m[38;2;125;239;19m█[39m[38;2;130;236;16m [39m[38;2;134;234;14m [39m[38;2;138;232;13m░[39m[38;2;142;229;11m█[39m[38;2;147;227;9m█[39m[38;2;151;224;8m [39m[38;2;155;221;6m [39m[38;2;159;218;5m [39m[38;2;163;215;4m░[39m[38;2;167;212;3m█[39m[38;2;171;209;2m█[39m[38;2;175;206;2m [39m[38;2;179;202;1m░[39m[38;2;183;199;1m█[39m[38;2;187;195;1m█[39m[38;2;190;192;1m [39m[38;2;194;188;1m░[39m[38;2;197;184;1m [39m[38;2;201;180;1m░[39m[38;2;204;177;2m█[39m[38;2;208;173;2m█[39m[38;2;211;169;3m [39m[38;2;214;165;4m [39m[38;2;217;161;5m░[39m[38;2;220;156;6m░[39m[38;2;223;152;7m [39m[38;2;226;148;9m░[39m[38;2;228;144;10m█[39m[38;2;231;140;12m█[39m[38;2;233;136;14m█[39m[38;2;236;131;16m█[39m[38;2;238;127;18m█[39m[38;2;240;123;20m█[39m[38;2;242;119;22m█[39m[38;2;243;114;25m[39m"
         << endl << "[38;2;51;253;78m░[39m[38;2;54;254;74m█[39m[38;2;58;254;71m█[39m[38;2;61;254;67m [39m[38;2;65;254;63m [39m[38;2;69;254;60m░[39m[38;2;72;254;56m█[39m[38;2;76;254;53m█[39m[38;2;80;253;49m░[39m[38;2;84;253;46m█[39m[38;2;88;252;43m█[39m[38;2;92;251;40m [39m[38;2;96;250;37m░[39m[38;2;100;249;34m█[39m[38;2;104;247;31m█[39m[38;2;109;246;28m [39m[38;2;113;244;26m [39m[38;2;117;242;23m [39m[38;2;121;240;21m [39m[38;2;125;239;19m [39m[38;2;130;236;16m░[39m[38;2;134;234;14m█[39m[38;2;138;232;13m█[39m[38;2;142;229;11m [39m[38;2;147;227;9m [39m[38;2;151;224;8m░[39m[38;2;155;221;6m█[39m[38;2;159;218;5m█[39m[38;2;163;215;4m [39m[38;2;167;212;3m [39m[38;2;171;209;2m [39m[38;2;175;206;2m░[39m[38;2;179;202;1m█[39m[38;2;183;199;1m█[39m[38;2;187;195;1m [39m[38;2;190;192;1m░[39m[38;2;194;188;1m█[39m[38;2;197;184;1m█[39m[38;2;201;180;1m [39m[38;2;204;177;2m [39m[38;2;208;173;2m [39m[38;2;211;169;3m░[39m[38;2;214;165;4m█[39m[38;2;217;161;5m█[39m[38;2;220;156;6m [39m[38;2;223;152;7m [39m[38;2;226;148;9m [39m[38;2;228;144;10m█[39m[38;2;231;140;12m█[39m[38;2;233;136;14m░[39m[38;2;236;131;16m█[39m[38;2;238;127;18m█[39m[38;2;240;123;20m░[39m[38;2;242;119;22m░[39m[38;2;243;114;25m░[39m[38;2;245;110;27m░[39m[38;2;247;106;30m [39m[38;2;248;102;33m[39m"
         << endl << "[38;2;61;254;67m░[39m[38;2;65;254;63m░[39m[38;2;69;254;60m█[39m[38;2;72;254;56m█[39m[38;2;76;254;53m█[39m[38;2;80;253;49m█[39m[38;2;84;253;46m█[39m[38;2;88;252;43m█[39m[38;2;92;251;40m░[39m[38;2;96;250;37m█[39m[38;2;100;249;34m█[39m[38;2;104;247;31m░[39m[38;2;109;246;28m█[39m[38;2;113;244;26m█[39m[38;2;117;242;23m█[39m[38;2;121;240;21m [39m[38;2;125;239;19m [39m[38;2;130;236;16m [39m[38;2;134;234;14m [39m[38;2;138;232;13m [39m[38;2;142;229;11m░[39m[38;2;147;227;9m█[39m[38;2;151;224;8m█[39m[38;2;155;221;6m [39m[38;2;159;218;5m [39m[38;2;163;215;4m░[39m[38;2;167;212;3m░[39m[38;2;171;209;2m█[39m[38;2;175;206;2m█[39m[38;2;179;202;1m█[39m[38;2;183;199;1m█[39m[38;2;187;195;1m█[39m[38;2;190;192;1m█[39m[38;2;194;188;1m [39m[38;2;197;184;1m░[39m[38;2;201;180;1m█[39m[38;2;204;177;2m█[39m[38;2;208;173;2m█[39m[38;2;211;169;3m [39m[38;2;214;165;4m [39m[38;2;217;161;5m [39m[38;2;220;156;6m░[39m[38;2;223;152;7m░[39m[38;2;226;148;9m█[39m[38;2;228;144;10m█[39m[38;2;231;140;12m█[39m[38;2;233;136;14m█[39m[38;2;236;131;16m█[39m[38;2;238;127;18m [39m[38;2;240;123;20m░[39m[38;2;242;119;22m░[39m[38;2;243;114;25m█[39m[38;2;245;110;27m█[39m[38;2;247;106;30m█[39m[38;2;248;102;33m█[39m[38;2;249;98;35m█[39m[38;2;250;94;38m█[39m[38;2;251;90;41m[39m"
         << endl << "[38;2;72;254;56m [39m[38;2;76;254;53m░[39m[38;2;80;253;49m░[39m[38;2;84;253;46m░[39m[38;2;88;252;43m░[39m[38;2;92;251;40m░[39m[38;2;96;250;37m░[39m[38;2;100;249;34m [39m[38;2;104;247;31m░[39m[38;2;109;246;28m░[39m[38;2;113;244;26m [39m[38;2;117;242;23m░[39m[38;2;121;240;21m░[39m[38;2;125;239;19m░[39m[38;2;130;236;16m [39m[38;2;134;234;14m [39m[38;2;138;232;13m [39m[38;2;142;229;11m [39m[38;2;147;227;9m [39m[38;2;151;224;8m [39m[38;2;155;221;6m░[39m[38;2;159;218;5m░[39m[38;2;163;215;4m [39m[38;2;167;212;3m [39m[38;2;171;209;2m [39m[38;2;175;206;2m [39m[38;2;179;202;1m░[39m[38;2;183;199;1m░[39m[38;2;187;195;1m░[39m[38;2;190;192;1m░[39m[38;2;194;188;1m░[39m[38;2;197;184;1m░[39m[38;2;201;180;1m [39m[38;2;204;177;2m [39m[38;2;208;173;2m░[39m[38;2;211;169;3m░[39m[38;2;214;165;4m░[39m[38;2;217;161;5m [39m[38;2;220;156;6m [39m[38;2;223;152;7m [39m[38;2;226;148;9m [39m[38;2;228;144;10m [39m[38;2;231;140;12m░[39m[38;2;233;136;14m░[39m[38;2;236;131;16m░[39m[38;2;238;127;18m░[39m[38;2;240;123;20m░[39m[38;2;242;119;22m [39m[38;2;243;114;25m [39m[38;2;245;110;27m [39m[38;2;247;106;30m░[39m[38;2;248;102;33m░[39m[38;2;249;98;35m░[39m[38;2;250;94;38m░[39m[38;2;251;90;41m░[39m[38;2;252;86;45m░[39m[38;2;253;82;48m [39m[38;2;254;78;51m[39m\n";
    
    cout << " Created by Guendouz Aimed \n youtube: www.youtube.com/@code0109 \n instagram: www.instagram.com/cod_e010\n";
    
    cout << "=========================================================\n\n"
         << "url : " << host << address << endl 
         << "wordlist : " << wordlistfile << " line : " << wordlistcount << endl 
         << "http method : " << http_method << "\n"
         << "thread : " << threadCounter << "\n\n"
         << "=========================================================\n\n";

    vector<thread> threads;
    cout << "\033[1;37m[-] brute-forcing directory: \n\033[0;37m";
    // Resolve the host name to an IP address
    struct addrinfo hints = {0};
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    struct addrinfo* result;
    if (sslOption == 1) port = "443"; else port = "80";
    int rc = getaddrinfo(host.c_str(), port.c_str(), &hints, &result);
    if (rc != 0) {
        std::cerr << "Error resolving hostname " << gai_strerror(rc) << "   " << port << host << std::endl;
        exit(1);
        return -1;
    }
    for (int i = 0; i < threadCounter; i++) {
        threads.emplace_back([&] {request(result->ai_addr, result->ai_addrlen);});
    }

    for(auto& t : threads) {
        t.join();
    }

    robotxt(host, sslOption);

    printf("\n\n\t\033[1;31mscaning completed");
    return 0;
}