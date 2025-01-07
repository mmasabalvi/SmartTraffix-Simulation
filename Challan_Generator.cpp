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
#include <signal.h>
#include <thread>
using namespace std;

enum PaymentStatus { PAID, UNPAID, OVERDUE };


class Challan 
{
public:

    int challanId;
    int vehicleNumber;
    string vehicleType;
    PaymentStatus paymentStatus;
    float totalAmount;
    string issueDateTime;
    string dueDateTime;

    Challan(int id, int vNumber, const string& vType, PaymentStatus status, float amount, 
            const string& issueDT, const string& dueDT)
        : challanId(id), vehicleNumber(vNumber), vehicleType(vType), paymentStatus(status), 
          totalAmount(amount), issueDateTime(issueDT), dueDateTime(dueDT) {}


    void setPaymentStatus(PaymentStatus status) 
    {
        paymentStatus = status;
    }
};


class ChallanGenerator
{
private:
    unordered_map<int, Challan*> challanMap; 			// Map challanId to Challan
    mutex challanMutex; 					// Mutex to protect challanMap

    int challanGenToUserPipeFd; 			// File descriptor for sending challans to UserPortal
    int smartTraffixPipeFd;      			// File descriptor for receiving challans from SmartTraffix
    int stripePaymentPipeFd;     			// File descriptor for receiving payments from StripePayment

    bool monitoringActive; 				// Flag to control thread loops

public:
    ChallanGenerator(int challanGenToUserFd, int smartTraffixFd, int stripePaymentFd)
        : challanGenToUserPipeFd(challanGenToUserFd),
          smartTraffixPipeFd(smartTraffixFd),
          stripePaymentPipeFd(stripePaymentFd),
          monitoringActive(true) {}

    ~ChallanGenerator() 
    {
        for (auto& pair : challanMap) 
        {
            delete pair.second;
        }
    }
    
    // Commented for now
    void printAllChallans() 
    {
        lock_guard<mutex> lock(challanMutex); // Ensure thread-safe access
        if (challanMap.empty())
         {
            cout << "ChallanGenerator: No challans to display." << endl;
            return;
        }

        cout << "=== All Received Challans ===" << endl;
        for (const auto& pair : challanMap) {
            Challan* challan = pair.second;
            cout << "Challan ID: " << challan->challanId << endl;
            cout << "Vehicle Number: " << challan->vehicleNumber << endl;
            cout << "Vehicle Type: " << challan->vehicleType << endl;
            cout << "Payment Status: " 
                      << (challan->paymentStatus == PAID ? "PAID" : 
                         (challan->paymentStatus == UNPAID ? "UNPAID" : "OVERDUE")) 
                      << endl;
            cout << "Total Amount: " << challan->totalAmount << " PKR" << endl;
            cout << "Issue Date-Time: " << challan->issueDateTime << endl;
            cout << "Due Date-Time: " << challan->dueDateTime << endl;
            cout << "-----------------------------" << endl;
        }
}

    void handleChallanData() 
    {
	char buffer[256];
    while (monitoringActive) 
    {
        ssize_t bytesRead = read(smartTraffixPipeFd, buffer, sizeof(buffer) - 1);
        if (bytesRead > 0) {
            buffer[bytesRead] = '\0';
            string challanData(buffer);
            

            stringstream ss(challanData);
            string line;
            while (getline(ss, line)) 
            {
                if (!line.empty()) {
                    processChallanLine(line);
                }
            }
        }
            else if (bytesRead == 0) 
            {

                close(smartTraffixPipeFd);
                smartTraffixPipeFd = open("/tmp/smarttraffix_to_challangen", O_RDONLY);
                if (smartTraffixPipeFd == -1) {
                    perror("ChallanGenerator: reopen smarttraffix_to_challangen for reading");
                    break;
                }
                
                else 
                {
                    cout << "ChallanGenerator: Reopened smarttraffix_to_challangen pipe for reading." << endl;
                }
            }
            
            
            else {
                perror("ChallanGenerator: read from smarttraffix_to_challangen");
                break;
            }
        }
    }


    void processChallanLine(const string& line)
     {
        // Expected challanDetails format: challanId|vehicleNumber|vehicleType|paymentStatus|totalAmount|issueDateTime|dueDateTime
        stringstream ss(line);
        string token;
        vector<string> tokens;

        while (getline(ss, token, '|'))
         {
            tokens.push_back(token);
        }

        if (tokens.size() != 7)
         {
            cerr << "ChallanGenerator: Invalid challan details format: " << line << endl;
            return;
        }


        int challanId = stoi(tokens[0]);
        int vehicleNumber = stoi(tokens[1]);
        string vehicleType = tokens[2];
        int paymentStatusInt = stoi(tokens[3]);
        float totalAmount = stof(tokens[4]);
        string issueDateTime = tokens[5];
        string dueDateTime = tokens[6];

        PaymentStatus paymentStatus;
        switch (paymentStatusInt) 
        {
            case PAID:
                paymentStatus = PAID;
                break;
            case UNPAID:
                paymentStatus = UNPAID;
                break;
            case OVERDUE:
                paymentStatus = OVERDUE;
                break;
            default:
                paymentStatus = UNPAID;
                break;
        }

        Challan* newChallan = new Challan(challanId, vehicleNumber, vehicleType, paymentStatus, 
                                         totalAmount, issueDateTime, dueDateTime);

        {
            lock_guard<mutex> lock(challanMutex);
            challanMap[challanId] = newChallan;
        }


        sendChallanToUserPortal(newChallan);

    cout << "=== New Challan Issued ===" << endl;
    cout << "Challan ID: " << challanId << endl;
    cout << "Vehicle Number: " << vehicleNumber << endl;
    cout << "Vehicle Type: " << vehicleType << endl;
    cout << "Payment Status: UNPAID" << endl;
    cout << "Total Amount: " << totalAmount << " PKR" << endl;
    cout << "Issue Date-Time: " << issueDateTime << endl;
    cout << "Due Date-Time: " << dueDateTime << endl;
    cout << "==========================" << endl << endl;
    }


    void sendChallanToUserPortal(Challan* challan) 
{
    float serviceCharge = challan->totalAmount * 0.17f; // Calculate 17% service fee

    stringstream ss;
    ss << challan->challanId << "|" 
       << challan->vehicleNumber << "|" 
       << challan->vehicleType << "|" 
       << challan->paymentStatus << "|" 
       << challan->totalAmount << "|" 
       << challan->issueDateTime << "|" 
       << challan->dueDateTime << "|"
       << serviceCharge << "\n";   // Append service charge as the 8th field

    string challanDetails = ss.str();
    ssize_t bytesWritten = write(challanGenToUserPipeFd, challanDetails.c_str(), challanDetails.size());
    if (bytesWritten == -1) 
    {
        perror("ChallanGenerator: write to challangen_to_userportal");
    } 
    else 
    {
        cout << "ChallanGenerator: Sent Challan ID " << challan->challanId 
             << " with service charge " << serviceCharge << " PKR to UserPortal." << endl;
    }
}


    void handlePayments() 
    {
        char buffer[256];
        
        while (monitoringActive)
         {
            ssize_t bytesRead = read(stripePaymentPipeFd, buffer, sizeof(buffer) - 1);
            if (bytesRead > 0) 
            {
                buffer[bytesRead] = '\0';
                string paymentData(buffer);


                stringstream ss(paymentData);
                string line;
                while (getline(ss, line)) 
                {
                    if (!line.empty()) {
                        processPaymentLine(line);
                    }
                }
            }
            
            else if (bytesRead == 0) 
            {
                close(stripePaymentPipeFd);
                stripePaymentPipeFd = open("/tmp/stripepayment_to_challangen", O_RDONLY);
                
                if (stripePaymentPipeFd == -1) 
                {
                    perror("ChallanGenerator: reopen stripepayment_to_challangen for reading");
                    break;
                }
                
                else 
                {
                    cout << "ChallanGenerator: Reopened stripepayment_to_challangen pipe for reading." << endl;
                }
            }
            
            else 
            {
                perror("ChallanGenerator: read from stripepayment_to_challangen");
                break;
            }
        }
    }


    void processPaymentLine(const string& line)
    {
        stringstream ss(line);
        string challanIdStr, paymentStatusStr;

        if (!getline(ss, challanIdStr, '|') || !getline(ss, paymentStatusStr, '|')) 
        {
            cerr << "ChallanGenerator: Invalid payment data format: " << line << endl;
            return;
        }

        int challanId = stoi(challanIdStr);
        PaymentStatus status;

        if (paymentStatusStr == "PAID") 
        {
            status = PAID;
        }
        else if (paymentStatusStr == "UNPAID")
        {
            status = UNPAID;
        }
        else if (paymentStatusStr == "OVERDUE")
        {
            status = OVERDUE;
        }
        
        else
        {
            cerr << "ChallanGenerator: Unknown payment status: " << paymentStatusStr << endl;
            return;
        }

        updatePaymentStatus(challanId, status);
    }


    void updatePaymentStatus(int challanId, PaymentStatus status) 
    {
        lock_guard<mutex> lock(challanMutex);
        
        if (challanMap.find(challanId) != challanMap.end())
         {
            challanMap[challanId]->setPaymentStatus(status);
            cout << "ChallanGenerator: Updated Challan ID " << challanId 
                      << " payment status to " 
                      << (status == PAID ? "PAID" : 
                         (status == UNPAID ? "UNPAID" : "OVERDUE")) 
                      << "." << endl;
        }
        
        else 
        {
            cerr << "ChallanGenerator: Challan ID " << challanId << " not found." << endl;
        }
    }


    bool stopMonitoring() 
    {
        lock_guard<mutex> lock(challanMutex); 
        return monitoringActive;
    }
    
     void periodicPrint(int intervalSeconds) 
     {
      	while (monitoringActive)
      	{
            		this_thread::sleep_for(chrono::seconds(intervalSeconds));

       	}
    }
};


void createNamedPipe(const char* pipeName) 
{

    struct stat st;
    if (stat(pipeName, &st) != 0) 
    {

        if (mkfifo(pipeName, 0666) != 0) {
            perror("mkfifo");
            exit(EXIT_FAILURE);
        }
        cout << "Named pipe created: " << pipeName << endl;
    }
    

}

int main() 
{

    createNamedPipe("/tmp/smarttraffix_to_challangen");
    createNamedPipe("/tmp/challangen_to_userportal");
    createNamedPipe("/tmp/stripepayment_to_challangen");


    int smartTraffixPipeFd = open("/tmp/smarttraffix_to_challangen", O_RDONLY);
    if (smartTraffixPipeFd == -1) 
    {
        perror("ChallanGenerator: open smarttraffix_to_challangen for reading");
        return EXIT_FAILURE;
    }


    int challanGenToUserPipeFd = open("/tmp/challangen_to_userportal", O_WRONLY);
    if (challanGenToUserPipeFd == -1) {
        perror("ChallanGenerator: open challangen_to_userportal for writing");
        return EXIT_FAILURE;
    }


    int stripePaymentPipeFd = open("/tmp/stripepayment_to_challangen", O_RDONLY);
    if (stripePaymentPipeFd == -1) {
        perror("ChallanGenerator: open stripepayment_to_challangen for reading");
        return EXIT_FAILURE;
    }


    ChallanGenerator challanGen(challanGenToUserPipeFd, smartTraffixPipeFd, stripePaymentPipeFd);


    thread challanHandlerThread(&ChallanGenerator::handleChallanData, &challanGen);


    thread paymentHandlerThread(&ChallanGenerator::handlePayments, &challanGen);
    thread printThread(&ChallanGenerator::periodicPrint, &challanGen, 60);


    cout << "ChallanGenerator is running. Press Enter to terminate." << endl;
    cin.get();

    challanGen.stopMonitoring();

    if (challanHandlerThread.joinable()) 
    {
        challanHandlerThread.join();
    }

    if (paymentHandlerThread.joinable()) 
    {
        paymentHandlerThread.join();
    }

    if (printThread.joinable()) 
    {
        printThread.join();
    }


    close(smartTraffixPipeFd);
    close(challanGenToUserPipeFd);
    close(stripePaymentPipeFd);

    cout << "ChallanGenerator terminated gracefully." << endl;

    return 0;
}

