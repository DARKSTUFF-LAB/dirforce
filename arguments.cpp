#include <iostream>
#include <cstring>
using namespace std;
string version = "v0.2";
void showhelp() {
    cout << "Version: " << version << "\n"
         << "Usage: dirforce -u/--url [url] -w/--wordlist [wordlist.txt]\n\n"
         << "\033[1;37mOptions: \033[0;37m\n"
         << "   -u or --url specify the url             (required)\n"
         << "   -w or --wordlist specify the wordlist   (required)\n"
         << "   -t or --thread specify how many thread to create\n"
         << "   -m or --method spedify the http method\n"
         << "   -r or --request specify to import the request from file\n"
         << "   -s or --suffixe to add a string in the begining of every path in the wordlist\n"
         << "   -p or --preffixe to add a string in the end of every path in the wordlist\n"
         << "\033[1;37mDocumentation:\033[0;37m https://github.com/guendouzaimed/dirforce\n"
         << "Please report any bug or issue in the tool.";
    exit(0);
}

int arguments(int argc, char* argv[], string *url, string *wordlist, int *threadCounter, int *sslOption, string *address, string *http_method, int *requestOption, string *readrequest, string *suffixe, string *preffixe)
{
    string urlV = "", wordlistV = "", addressV = "", http_methodV = "HEAD", readrequestV = "", suffixeV = "", preffixeV = "";
    int sslOptionV = 0, threadCounterV = 30, requestOptionV = 0;
    
    //std::cout << "Number of arguments: " << argc << std::endl;

    for (int i = 1; i < argc; i++)
    {
        if (strncmp(argv[i], "-u", 2) == 0 || strncmp(argv[i], "--url", 5) == 0)
        {
            //std::cout << "Argument: " << argv[i] << std::endl;
            if (i + 1 < argc) {
                //std::cout << "Value: " << argv[i + 1] << std::endl;
                urlV = argv[i + 1];
                i++;
            }
        }
        
        if (strncmp(argv[i], "-w", 2) == 0 || strncmp(argv[i], "--wordlist", 10) == 0)
        {
            if (i + 1 < argc)
            {
                wordlistV = argv[i + 1];
                i++;
            }
        }

        if (strncmp(argv[i], "-t", 2) == 0 || strncmp(argv[i], "--thread", 8) == 0)
        {
            if (i + 1 < argc)
            {
                threadCounterV = stoi(argv[i + 1]);
                i++;
            }
        }

        if (strncmp(argv[i], "-m", 2) == 0 || strncmp(argv[i], "--method", 8) == 0)
        {
            if (i + 1 < argc)
            {
                http_methodV = argv[i + 1];
                i++;
            }
        }

        if (strncmp(argv[i], "-r", 2) == 0 || strncmp(argv[i], "--request", 9) == 0)
        {
            if (i + 1 < argc)
            {
                requestOptionV = 1;
                string line;
                ifstream requestfile(argv[i+1]);
                while (getline(requestfile, line)) {
                    readrequestV += line + "\n";
                }
                readrequestV += "\n";
                i++;
            }
        }

        if (strncmp(argv[i], "-s", 2) == 0 || strncmp(argv[i], "--suffixe", 9) == 0)
        {
            if (i + 1 < argc)
            {
                suffixeV = argv[i + 1];
                i++;
            }
        }

        if (strncmp(argv[i], "-p", 2) == 0 || strncmp(argv[i], "--preffixe", 10) == 0)
        {
            if (i + 1 < argc)
            {
                preffixeV = argv[i + 1];
                i++;
            }
        }

        if (strncmp(argv[i], "-h", 2) == 0 || strncmp(argv[i], "--help", 6) == 0)
        {
            showhelp();
        }

    }
    if (urlV.find("https") == string::npos) {
        if (urlV.find("http") == string::npos) {
            sslOptionV = 0;
        } else {
            sslOptionV = 0;
            urlV = urlV.substr(7);
        }
    } else {
        sslOptionV = 1;
        urlV = urlV.substr(8);
    }

    size_t slash_index = urlV.find("/");

    if(slash_index == string::npos) {
        //urlV = urlV
        addressV = "/";
    } else if (slash_index == urlV.size() - 1) {
        urlV = urlV.substr(0, slash_index);
        addressV = "/";
        //cout << "2";
    } else if (slash_index != urlV.size()) {
        if (urlV[urlV.size() - 1] == '/') {//4
            addressV = urlV.substr(slash_index);
            urlV = urlV.substr(0, slash_index);
            //cout << "4";
        } else {
            //cout << "3";
            addressV = urlV.substr(slash_index) + "/";
            urlV = urlV.substr(0, slash_index);
        }
    }
    //cout << endl << addressV << endl << urlV << endl;
    *url = urlV;
    *address = addressV;
    *threadCounter = threadCounterV;
    *sslOption = sslOptionV;
    *wordlist = wordlistV;
    *http_method = http_methodV;
    *requestOption = requestOptionV;
    *readrequest = readrequestV;
    *suffixe = suffixeV;
    *preffixe = preffixeV;
    return 0;
}
