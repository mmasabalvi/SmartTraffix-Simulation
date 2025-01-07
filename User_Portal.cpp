#include <iostream>
#include <fstream>
#include <string>
#include <cstring>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <ctime>
#include <sstream>
#include <unordered_map>
#include <vector>
#include <mutex>
#include <thread>
#include <chrono>
using namespace std;

enum PaymentStatus { PAID, UNPAID, OVERDUE };

class Challan 
{
public:
    int challanId;
    int vehicleNumber;
    string vehicleType;
    PaymentStatus paymentStatus = UNPAID;
    float totalAmount;
    string issueDateTime;
    string dueDateTime;

    Challan(int id, int vNumber, const string& vType, PaymentStatus status, float amount, 
            const string& issueDT, const string& dueDT)
        : challanId(id), vehicleNumber(vNumber), vehicleType(vType), paymentStatus(status), 
          totalAmount(amount), issueDateTime(issueDT), dueDateTime(dueDT) {}

    void setPaymentStatus(PaymentStatus status) {
        paymentStatus = status;
    }
};

class UserPortal {
private:
    unordered_map<int, Challan*> challanMap; 		// Map challanId to Challan
    mutex portalMutex; 					// Protects challanMap

public:
    ~UserPortal() {
        for (auto& pair : challanMap) {
            delete pair.second;
        }
    }


    void receiveChallanDetails(const string& challanDetails)
     {
        // Format: challanId|vehicleNumber|vehicleType|paymentStatus|totalAmount|issueDateTime|dueDateTime|serviceCharge
        stringstream ss(challanDetails);
        string token;
        vector<string> tokens;

        while (getline(ss, token, '|')) 
        {
            tokens.push_back(token);
        }

        if (tokens.size() != 8)
         {
            // Invalid format
            return;
        }

        int challanId = stoi(tokens[0]);
        int vehicleNumber = stoi(tokens[1]);
        string vehicleType = tokens[2];
        int paymentStatusInt = stoi(tokens[3]);
        float totalAmount = stof(tokens[4]);
        string issueDateTime = tokens[5];
        string dueDateTime = tokens[6];
        float serviceCharge = stof(tokens[7]); 

        PaymentStatus paymentStatus;
        switch (paymentStatusInt) 
        {
            case PAID:   paymentStatus = PAID; break;
            case UNPAID: paymentStatus = UNPAID; break;
            case OVERDUE:paymentStatus = OVERDUE; break;
            default:     paymentStatus = UNPAID; break;
        }

        Challan* newChallan = new Challan(challanId, vehicleNumber, vehicleType, paymentStatus, totalAmount, issueDateTime, dueDateTime);
        {
            lock_guard<mutex> lock(portalMutex);
            challanMap[challanId] = newChallan;
        }
    }


    void viewChallansByVehicleNumber(int vehicleNumber)
    {
        lock_guard<mutex> lock(portalMutex);
        bool found = false;

        cout << "\n=== Challan Details for Vehicle Number: " << vehicleNumber << " ===" << endl;
        for (const auto& pair : challanMap) {
            Challan* challan = pair.second;
            if (challan->vehicleNumber == vehicleNumber) {
                found = true;
                cout << "----------------------------------------" << endl;
                cout << "Challan ID:       " << challan->challanId << endl;
                cout << "Vehicle Number:   " << challan->vehicleNumber << endl;
                cout << "Vehicle Type:     " << challan->vehicleType << endl;
                cout << "Amount (PKR):     " << challan->totalAmount << endl;
                cout << "Service Charge:   " << (challan->totalAmount * 0.17f) << " PKR" << endl;
                cout << "Payment Status:   " 
                     << (challan->paymentStatus == PAID ? "PAID" :
                        (challan->paymentStatus == UNPAID ? "UNPAID" : "OVERDUE"))
                     << endl;
                cout << "Issue Date-Time:  " << challan->issueDateTime << endl;
                cout << "Due Date-Time:    " << challan->dueDateTime << endl;
            }
        }

        if (!found) 
        {
            cout << "No challans found for vehicle number " << vehicleNumber << "." << endl;
        }
        cout << "========================================" << endl << endl;
    }
};


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
    createNamedPipe("/tmp/challangen_to_userportal");

    int challanGenToUserPipeFd = open("/tmp/challangen_to_userportal", O_RDONLY | O_NONBLOCK);
    if (challanGenToUserPipeFd == -1) 
    {
        perror("UserPortal: open challangen_to_userportal for reading");
        return EXIT_FAILURE;
    }

    UserPortal userPortal;

    //background thread to continuously read from the pipe and store data
    bool running = true;
    thread readerThread([&]() 
    {
        char buffer[512];
        
        while (running) 
        {
            ssize_t bytesRead = read(challanGenToUserPipeFd, buffer, sizeof(buffer) - 1);
            if (bytesRead > 0)
             {
                buffer[bytesRead] = '\0'; 
                string challanDetails(buffer);

                // Process each line
                stringstream ss(challanDetails);
                string line;
                
                while (getline(ss, line)) 
                {
                    if (!line.empty())
                    {
                        userPortal.receiveChallanDetails(line);
                    }
                }
            } 
            
            else if (bytesRead == -1) 
            {
                // If no data is currently available, just sleep a bit and retry
                if (errno == EAGAIN || errno == EWOULDBLOCK) {
                    this_thread::sleep_for(chrono::milliseconds(100));
                    continue;
                } else {
                    perror("UserPortal: read error");
                    running = false;
                }
            } 
            
            else 
            {
                // bytesRead == 0 means If the writer closed, we can stop
                // Or continue waiting if new writers EXPECTED later.
                // For now, just sleep and retry (simulate waiting for new data).
                this_thread::sleep_for(chrono::seconds(1));
            }
        }
    });

    // Main thread: allow user to query at any time
    cout << "UserPortal running. Enter vehicle numbers to view their challans (0 to exit):" << endl;
    while (true) 
    {
        int userVehicleNumber;
        cout << "Enter Vehicle Number: ";
        cin >> userVehicleNumber;
        if (!cin || userVehicleNumber == 0) 
        {
            break;
        }

        userPortal.viewChallansByVehicleNumber(userVehicleNumber);
    }


    running = false;
    if (readerThread.joinable())
        readerThread.join();
    
    close(challanGenToUserPipeFd);

    return 0;
}

