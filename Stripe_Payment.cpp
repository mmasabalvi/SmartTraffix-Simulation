#include <iostream>
#include <string>
#include <cstring>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <sstream>
#include <thread>
using namespace std;

enum PaymentStatus { PAID, UNPAID, OVERDUE };


void createNamedPipe(const char* pipeName) 
{

    struct stat st;
    if (stat(pipeName, &st) != 0) 
    {
        if (mkfifo(pipeName, 0666) != 0) 
        {
            perror("mkfifo");
            exit(EXIT_FAILURE);
        }
        
        cout << "Named pipe created: " << pipeName << endl;
    }
}

int main() 
{

    createNamedPipe("/tmp/stripepayment_to_challangen");

    int stripeToChallanPipeFd = open("/tmp/stripepayment_to_challangen", O_WRONLY);
    
    if (stripeToChallanPipeFd == -1) 
    {
        perror("StripePayment: open stripepayment_to_challangen for writing");
        return EXIT_FAILURE;
    }

    while (true) 
    {
        int challanId;
        cout << "StripePayment: Enter Challan ID to pay (0 to exit): ";
        cin >> challanId;
        
        if (challanId == 0) 
        {
            break;
        }


        cout << "StripePayment: Processing payment for Challan ID " << challanId << "..." << endl;
        this_thread::sleep_for(chrono::seconds(2)); // Simulate delay


        stringstream ss;
        ss << challanId << "|" << "PAID" << "\n"; 

        string paymentData = ss.str();

        ssize_t bytesWritten = write(stripeToChallanPipeFd, paymentData.c_str(), paymentData.size());
        
        if (bytesWritten == -1) 
        {
            perror("StripePayment: write to stripepayment_to_challangen");
        }
        
        else 
        {
            cout << "StripePayment: Payment for Challan ID " << challanId << " processed and notified." << endl;
        }
    }

    close(stripeToChallanPipeFd);

    return 0;
}

