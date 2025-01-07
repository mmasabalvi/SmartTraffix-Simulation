#include <SFML/Graphics.hpp>
#include <random>
#include <ctime>
#include <cstring>
#include <iostream>
#include <thread>
#include <vector>
#include <mutex>
#include <chrono>
#include <atomic>
#include <sstream>
#include <fstream>
#include <iomanip> 
#include <functional>
#include <queue>
#include <map>
#include <signal.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string>
#include <unistd.h>

using namespace std;
using namespace sf;

const int numThreads = thread::hardware_concurrency(); 

//5 minutes
const float TIME_SCALE = 86400.0f / 300.0f;
int simulatedDay = 1; // Starting from Day 1


struct Speed {
    const char* type;
    float value;
};

const Speed speeds[] = {
    {"car", 2.25f},
    {"bus", 1.8f},
    {"truck", 1.8f},
    {"bike", 2.5f}
};

struct StartingPosition {
    const char* direction;
    float x[3]; 			
    float y[3]; 			
};

const StartingPosition positions[] = {
    {"right", {0, 0}, {260, 310}},       // Rightward movement
    {"down", {800, 712}, {0, 0}},       // Downward movement
    {"left", {1250, 1250}, {420, 372}}, // Leftward movement
    {"up", {490, 582}, {700, 700}}      // Upward movement
};

enum Direction { Right, Down, Left, Up };

Direction parseDirection(const char* dir) {
    if (strcmp(dir, "right") == 0) return Right;
    if (strcmp(dir, "down") == 0) return Down;
    if (strcmp(dir, "left") == 0) return Left;
    return Up;
}

class TrafficLight {
public:
    enum State { RED, YELLOW, GREEN };
    State state;

    Sprite sprite;
    Texture redTexture, yellowTexture, greenTexture;

    TrafficLight(float x, float y) {
        if (!redTexture.loadFromFile("assets/images/signals/red.png") ||
            !yellowTexture.loadFromFile("assets/images/signals/yellow.png") ||
            !greenTexture.loadFromFile("assets/images/signals/green.png")) {
            throw runtime_error("Failed to load traffic light textures.");
        }

        state = RED;
        sprite.setTexture(redTexture);
        sprite.setPosition(x, y);
    }

    void setState(State newState) {
        state = newState;
        switch (state) {
            case RED: sprite.setTexture(redTexture); break;
            case YELLOW: sprite.setTexture(yellowTexture); break;
            case GREEN: sprite.setTexture(greenTexture); break;
        }
    }

    bool canProceed() const {
        return state == GREEN;
    }

    void draw(RenderWindow& window) {
        window.draw(sprite);
    }
};

mutex vehiclesMutex;
mutex analyticsMutex;
int vehicleCount = 0; 

enum VehicleType { LightVehicle, HeavyVehicle, EmergencyVehicle };
ofstream vehicleLogFile("vehicle_log.txt");
class Vehicle {
public:
    Sprite sprite;
    float speed;
    Direction direction;
    int lane;
    VehicleType type; 
    static int nextNumberPlate;
    int numberPlate;
    Clock arrivalClock;
    bool logged; 
    bool Challan_Status;
    bool hasCrossedStopLine = false;
    Clock speedIncrementClock;

    Vehicle(const Texture& texture, float startX, float startY, float speed, int lane, Direction direction, VehicleType type)
        : speed(speed), lane(lane), direction(direction), type(type), logged(false), Challan_Status(false) {
        sprite.setTexture(texture);
        sprite.setPosition(startX, startY);
        numberPlate = nextNumberPlate++;
        arrivalClock.restart();      
        speedIncrementClock.restart(); 
    }

    void logArrival(const char* entryPoint) {
        if (vehicleLogFile.is_open()) {
            float arrivalTime = arrivalClock.getElapsedTime().asSeconds();
            string vehicleTypeString;
            switch (type) {
                case LightVehicle: vehicleTypeString = "Light"; break;
                case HeavyVehicle: vehicleTypeString = "Heavy"; break;
                case EmergencyVehicle: vehicleTypeString = "Emergency"; break;
            }
            
            vehicleLogFile << numberPlate << ","
                           << fixed << setprecision(2) << speed << ","
                           << fixed << setprecision(2) << arrivalTime << ","
                           << entryPoint << ","
                           << vehicleTypeString << endl;
        } else {
            cerr << "Error: Unable to open vehicle log file." << endl;
        }
    }

    void move(Vehicle* vehicles[], int vehicleCount, const map<Direction, TrafficLight>& trafficLights) {
        const float safeDistance = 60.0f;
        Vector2f myPos = this->sprite.getPosition();

        map<Direction, float> stopLines = {
            {Right, 300.0f},
            {Down, 160.0f},
            {Left, 930.0f},
            {Up, 490.0f}
        };

        if (!hasCrossedStopLine) {
            if ((this->direction == Right && myPos.x > stopLines[Right] + safeDistance) ||
                (this->direction == Down && myPos.y > stopLines[Down] + safeDistance) ||
                (this->direction == Left && myPos.x < stopLines[Left] - safeDistance) ||
                (this->direction == Up && myPos.y < stopLines[Up] - safeDistance)) {
                hasCrossedStopLine = true;
            }
        }

        bool shouldStop = false;

        if (!hasCrossedStopLine && trafficLights.at(this->direction).state != TrafficLight::GREEN) {
            if ((this->direction == Right && myPos.x >= stopLines[Right]) ||
                (this->direction == Down && myPos.y >= stopLines[Down]) ||
                (this->direction == Left && myPos.x <= stopLines[Left]) ||
                (this->direction == Up && myPos.y <= stopLines[Up])) {
                shouldStop = true;
            }
        }

        for (int i = 0; i < vehicleCount; ++i) {
            if (vehicles[i] == this) continue; 
            if (vehicles[i] != nullptr && vehicles[i]->lane == this->lane && vehicles[i]->direction == this->direction) {
                Vector2f otherPos = vehicles[i]->sprite.getPosition();

                if (this->direction == Right && myPos.x < otherPos.x && (otherPos.x - myPos.x) < safeDistance) {
                    shouldStop = true;
                    break;
                } else if (this->direction == Down && myPos.y < otherPos.y && (otherPos.y - myPos.y) < safeDistance) {
                    shouldStop = true;
                    break;
                } else if (this->direction == Left && myPos.x > otherPos.x && (myPos.x - otherPos.x) < safeDistance) {
                    shouldStop = true;
                    break;
                } else if (this->direction == Up && myPos.y > otherPos.y && (myPos.y - otherPos.y) < safeDistance) {
                    shouldStop = true;
                    break;
                }
            }
        }

        if (shouldStop) {
            return;
        }

        switch (direction) {
            case Right: sprite.move(speed, 0); break;
            case Down: sprite.move(0, speed); break;
            case Left: sprite.move(-speed, 0); break;
            case Up: sprite.move(0, -speed); break;
        }
        
        if (speedIncrementClock.getElapsedTime().asSeconds() >= 5.0f) {
            float speedIncrement = 1.0f; 
            speed += speedIncrement;
            speedIncrementClock.restart();
            cout << "Vehicle ID " << numberPlate << " speed increased to " << speed * 10 << " km/h." << endl;
        }
    }

    float getMaxSpeed() const {
        switch (type) {
            case LightVehicle:
                return 6.0f; // 60 km/h
            case HeavyVehicle:
                return 4.0f; // 40 km/h
            case EmergencyVehicle:
                return 8.0f; // 80 km/h
        }
        return 6.0f;
    }
};

int Vehicle::nextNumberPlate = 1; 

struct TrafficAnalytics {
    int totalLightVehicles = 0;
    int totalHeavyVehicles = 0;
    int totalEmergencyVehicles = 0;
    int totalActiveChallans = 0;
    vector<string> vehiclesWithChallans; 
};

TrafficAnalytics analytics;

// Named Pipe creation
void createNamedPipe(const char* pipeName) {
    struct stat st;
    if (stat(pipeName, &st) != 0) {
        if (mkfifo(pipeName, 0666) != 0) {
            perror("mkfifo");
            exit(EXIT_FAILURE);
        }
        cout << "Named pipe created: " << pipeName << endl;
    }
}


Clock simulationClock;

void updateSimulationTime(Clock& clock, int& hour, int& minute, int& day) {
    const float secondsPerDay = 60.0f; 
    float elapsedSeconds = clock.getElapsedTime().asSeconds();
    float secondsPerSimulatedMinute = secondsPerDay / (24 * 60);

    int totalSimulatedMinutes = static_cast<int>(elapsedSeconds / secondsPerSimulatedMinute);
    day = (totalSimulatedMinutes / (24 * 60)) + 1; // Incrementing day
    hour = (currentMinute / 60) % 24;
    minute = currentMinute % 60;
}



string formatTime(int hour, int minute) {
    ostringstream oss;
    oss << "Time: " << hour << ":";
    if (minute < 10) {
        oss << "0"; 
    }
    oss << minute;
    return oss.str();
}

bool isPeakHours(int hour, int minute) {
    int timeInMinutes = hour * 60 + minute;
    return (timeInMinutes >= 7 * 60 && timeInMinutes <= 9 * 60 + 30) || 
           (timeInMinutes >= 16 * 60 + 30 && timeInMinutes <= 20 * 60 + 30);
}


struct ResourceState 
{
    map<string,int> available;
    map<string,int> allocated;
    map<string,int> need;
};

ResourceState resourceState;


bool isSafeState(const ResourceState& state) {
    //customers lanes
    vector<string> customers;
    for (auto& n : state.need) {
        customers.push_back(n.first);
    }

    //total available resources in the system
    int totalAvailable = 0;
    for (auto& av : state.available) {
        totalAvailable += av.second;
    }

    //Initialize finish flags
    map<string, bool> finish;
    for (auto& c : customers) finish[c] = false;

    //'work' represents the number of units available for allocation
    int work = totalAvailable;
    int countFinish = 0; 


    bool found = true;
    while (found) {
        found = false;
        for (auto& c : customers)
         {
            if (!finish[c]) 
            {
                int needC = state.need.at(c);

                if (needC <= work) 
                {
                    work += state.allocated.at(c); 
                    finish[c] = true;
                    found = true;
                    countFinish++;
                }
            }
        }
    }


    return (countFinish == (int)customers.size());
}

map<string, queue<Vehicle*>> laneQueues; // Lane-specific queues

// Convert direction and lane index into a string key
string getLaneName(Direction dir, int laneIdx) 
{
    string laneName;
    switch (dir) 
    {
        case Right: laneName = (laneIdx == 0) ? "east_lane0" : "east_lane1"; break;
        case Left:  laneName = (laneIdx == 0) ? "west_lane0" : "west_lane1"; break;
        case Up:    laneName = (laneIdx == 0) ? "north_lane0" : "north_lane1"; break;
        case Down:  laneName = (laneIdx == 0) ? "south_lane0" : "south_lane1"; break;
    }
    return laneName;
}

// Banker function
void Banker(Vehicle* vehicle, const string& lane)
 {
    if (resourceState.available[lane] > 0 && resourceState.need[lane] > 0) 
    {

        resourceState.available[lane]--;
        resourceState.allocated[lane]++;
        resourceState.need[lane]--;

        if (isSafeState(resourceState)) 
        {
            if (vehicle->type == EmergencyVehicle)
             {
                queue<Vehicle*> tempQueue;
                tempQueue.push(vehicle);
                while (!laneQueues[lane].empty())
                 {
                    tempQueue.push(laneQueues[lane].front());
                    laneQueues[lane].pop();
                }
                laneQueues[lane] = move(tempQueue);
            } 
            
            else
             {
                laneQueues[lane].push(vehicle);
            }
        } 
        
        else 
        {
            // Revert allocation
            resourceState.available[lane]++;
            resourceState.allocated[lane]--;
            resourceState.need[lane]++;
            delete vehicle; // Reject the vehicle
        }
    } 
    
    else 
    {
        // No resources available or need not met
        delete vehicle;
    }
}


void processLaneQueues(Vehicle* vehicles[], int& vehicleCount, int maxVehicles) {
    for (auto& entry : laneQueues) {
        const string& lane = entry.first;
        auto& q = entry.second;


        while (!q.empty() && vehicleCount < maxVehicles) {
            Vehicle* v = q.front();
            q.pop();
            vehicles[vehicleCount++] = v;

        }
    }
}


atomic<bool> monitoringActive(true); 

void monitorVehicleSpeeds(Vehicle* vehicles[], mutex& vehiclesMutex, int& challanCounter, int challanPipeFd) {
    while (monitoringActive) {
        {
            lock_guard<mutex> lock(vehiclesMutex);

            for (int i = 0; i < vehicleCount; ++i) {
                Vehicle* v = vehicles[i];
                if (v != nullptr && !v->Challan_Status) {
                    float vehicleSpeedLimit;
                    switch (v->type) {
                        case LightVehicle: vehicleSpeedLimit = 6.0f; break; 
                        case HeavyVehicle: vehicleSpeedLimit = 4.0f; break;
                        case EmergencyVehicle: vehicleSpeedLimit = 8.0f; break;
                    }

                    if (v->speed > vehicleSpeedLimit) {
                        int challanId = challanCounter++;
                        time_t now = time(nullptr);
                        char buf[100];
                        strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", localtime(&now));
                        string issueDateTime(buf);

                        tm* tm_info = localtime(&now);
                        tm_info->tm_mday += 30;
                        mktime(tm_info);
                        strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", tm_info);
                        string dueDateTime(buf);

                        int paymentStatus = 0; 
                        float totalAmount;
                        switch (v->type) {
                            case LightVehicle: totalAmount = 1500.0f; break;
                            case HeavyVehicle: totalAmount = 2500.0f; break;
                            case EmergencyVehicle: totalAmount = 1000.0f; break;
                        }

                        stringstream ss;
                        ss << challanId << "|" << v->numberPlate << "|"
                           << (v->type == LightVehicle ? "Light" : (v->type == HeavyVehicle ? "Heavy" : "Emergency"))
                           << "|" << paymentStatus << "|" << totalAmount << "|" << issueDateTime << "|" << dueDateTime << "\n";

                        string challanData = ss.str();
                        ssize_t bytesWritten = write(challanPipeFd, challanData.c_str(), challanData.size());
                        if (bytesWritten == -1) {
                            perror("write");
                        }

                        v->Challan_Status = true;
                        analytics.totalActiveChallans++;

                        // Free the lane resource since vehicle is removed
                        string laneName = getLaneName(v->direction, v->lane);
                        resourceState.available[laneName]++;
                        resourceState.allocated[laneName]--;

                        delete v;
                        vehicles[i] = vehicles[vehicleCount - 1];
                        vehicles[vehicleCount - 1] = nullptr;
                        vehicleCount--;
                        i--;
                    }
                }
            }
        }
        this_thread::sleep_for(chrono::milliseconds(100));
    }
}

void updateVehicleSubset(Vehicle* vehicles[], int start, int end, float deltaTime, const map<Direction, TrafficLight>& trafficLights) {
    for (int i = start; i < end; ++i) {
        if (vehicles[i] != nullptr) {
            vehicles[i]->move(vehicles, end, trafficLights);
        }
    }
}

void updateTrafficLights(map<Direction, TrafficLight>& trafficLights, float deltaTime, 
                         Vehicle* vehicles[], int vehicleCount,
                         const map<string, queue<Vehicle*>>& laneQueues) 
{
    static float timer = 0;
    static int roundRobinCounter = 0;  

    timer += deltaTime;

    bool emergencyFound = false;
    Direction emergencyDir = Up;

    // Check for emergency vehicles
    for (int i = 0; i < vehicleCount; i++) 
    {
        if (vehicles[i] && vehicles[i]->type == EmergencyVehicle) {
            emergencyFound = true;
            emergencyDir = vehicles[i]->direction;
            break;
        }
    }

    if (!emergencyFound)
     {
        auto laneToDirection = [](const string& laneName) -> Direction
         {
            if (laneName.find("east") != string::npos) return Right;
            if (laneName.find("west") != string::npos) return Left;
            if (laneName.find("north") != string::npos) return Up;
            if (laneName.find("south") != string::npos) return Down;
            return Up;
        };

        for (const auto& [laneName, q] : laneQueues) 
        {
            if (!q.empty()) {
                Vehicle* frontVeh = q.front();
                if (frontVeh && frontVeh->type == EmergencyVehicle) {
                    emergencyFound = true;
                    emergencyDir = laneToDirection(laneName);
                    break;
                }
            }
        }
    }

    // emergency is found
    if (emergencyFound) 
    {
        for (auto& [direction, light] : trafficLights) 
        {
            light.setState(TrafficLight::RED);
        }
        trafficLights.at(emergencyDir).setState(TrafficLight::GREEN);
        timer = 0; 
        return;
    }

    if (timer > 10.0f) 
    {
        for (auto& [direction, light] : trafficLights) 
        {
            light.setState(TrafficLight::RED);
        }

        switch (roundRobinCounter % 4)
         {
            case 0:
                trafficLights.at(Up).setState(TrafficLight::GREEN);
                trafficLights.at(Left).setState(TrafficLight::YELLOW);
                break;
            case 1:
                trafficLights.at(Left).setState(TrafficLight::GREEN);
                trafficLights.at(Down).setState(TrafficLight::YELLOW);
                break;
            case 2:
                trafficLights.at(Down).setState(TrafficLight::GREEN);
                trafficLights.at(Right).setState(TrafficLight::YELLOW);
                break;
            case 3:
                trafficLights.at(Right).setState(TrafficLight::GREEN);
                trafficLights.at(Up).setState(TrafficLight::YELLOW);
                break;
        }
        
        roundRobinCounter++;
        timer = 0;
    }
}

void drawVehicles(RenderWindow& window, Vehicle* vehicles[], int& vehicleCount, float deltaTime, const map<Direction, TrafficLight>& trafficLights) {
    for (int i = 0; i < vehicleCount;) {
        if (vehicles[i] != nullptr) {
            vehicles[i]->move(vehicles, vehicleCount, trafficLights);
            window.draw(vehicles[i]->sprite);

            FloatRect vehicleBounds = vehicles[i]->sprite.getGlobalBounds();
            
            
            
if ((vehicleBounds.left + vehicleBounds.width < 0 ||
     vehicleBounds.top + vehicleBounds.height < 0 ||
     vehicleBounds.left > window.getSize().x ||
     vehicleBounds.top > window.getSize().y) && !vehicles[i]->logged) {
    const char* entryPoint = "";
    switch (vehicles[i]->direction) {
        case Right: entryPoint = "East"; break;
        case Down: entryPoint = "South"; break;
        case Left: entryPoint = "West"; break;
        case Up: entryPoint = "North"; break;
    }
    vehicles[i]->logArrival(entryPoint);
    vehicles[i]->logged = true;

    // Free lane resource
    string laneName = getLaneName(vehicles[i]->direction, vehicles[i]->lane);
    resourceState.available[laneName]++;
    resourceState.allocated[laneName]--;

    resourceState.need[laneName] = 10;

    delete vehicles[i];
    vehicles[i] = vehicles[vehicleCount - 1];
    vehicles[vehicleCount - 1] = nullptr;
    vehicleCount--;
} else {
    ++i;
}

        } 
        
        else 
        {
            ++i;
        }
    }
}


void updateVehiclesThreaded(Vehicle* vehicles[], int vehicleCount, float deltaTime, const map<Direction, TrafficLight>& trafficLights)
 {
    thread threads[numThreads];
    int vehiclesPerThread = vehicleCount / numThreads;
    int remainder = vehicleCount % numThreads;

    int start = 0;
    for (int i = 0; i < numThreads; ++i) 
    {
        int end = start + vehiclesPerThread + (i < remainder ? 1 : 0);
        threads[i] = thread(updateVehicleSubset, vehicles, start, end, deltaTime, cref(trafficLights));
        start = end;
    }

    for (int i = 0; i < numThreads; ++i) 
    {
        threads[i].join();
    }
}

void runInThread(const function<void()>& func) {
    thread t(func);
    t.detach(); 
}

void displayAnalytics(RenderWindow& window, const TrafficAnalytics& analytics) {
    Font font;
    if (!font.loadFromFile("assets/fonts/arial_narrow_7.ttf")) {
        cerr << "Error: Unable to load font." << endl;
    }

    Text analyticsText;
    analyticsText.setFont(font);
    analyticsText.setCharacterSize(18);
    analyticsText.setFillColor(Color::White);

    stringstream ss;
    ss << "Total Light Vehicles: " << analytics.totalLightVehicles << "\n";
    ss << "Total Heavy Vehicles: " << analytics.totalHeavyVehicles << "\n";
    ss << "Total Emergency Vehicles: " << analytics.totalEmergencyVehicles << "\n";
    ss << "Active Challans: " << analytics.totalActiveChallans << "\n";


    analyticsText.setString(ss.str());
    analyticsText.setPosition(0,0);
    window.draw(analyticsText);
}



void generateLightVehicle(Vehicle* vehicles[], int& vehicleCount, const Texture textures[], int maxVehicles, Direction dir) {
    if (vehicleCount >= maxVehicles) return;

    int laneIdx = rand() % 2; 
    float startX = positions[dir].x[laneIdx];
    float startY = positions[dir].y[laneIdx];

    const float lightVehicleSpeedRange[] = {2.0f, 6.0f}; 
    float speed = lightVehicleSpeedRange[0] + static_cast<float>(rand()) / (RAND_MAX / (lightVehicleSpeedRange[1] - lightVehicleSpeedRange[0]));

    Vehicle* newVehicle = new Vehicle(
        textures[dir * 5 + 0],
        startX, startY,
        speed,
        laneIdx,
        dir,
        LightVehicle
    );

    // Assign resources via Banker
    string laneName = getLaneName(dir, laneIdx);
    Banker(newVehicle, laneName);
}

void generateHeavyVehicle(Vehicle* vehicles[], int& vehicleCount, const Texture textures[], int maxVehicles, Direction dir) {
    if (vehicleCount >= maxVehicles) return;

    // Heavy vehicles always lane 1
    int laneIdx = 1;
    float startX = positions[dir].x[laneIdx];
    float startY = positions[dir].y[laneIdx];

    const float heavyVehicleSpeedRange[] = {1.5f, 4.0f};
    float speed = heavyVehicleSpeedRange[0] + static_cast<float>(rand()) / (RAND_MAX / (heavyVehicleSpeedRange[1] - heavyVehicleSpeedRange[0]));

    Vehicle* newVehicle = new Vehicle(
        textures[dir * 5 + 2],
        startX, startY,
        speed,
        laneIdx,
        dir,
        HeavyVehicle
    );

    string laneName = getLaneName(dir, laneIdx);
    Banker(newVehicle, laneName);
}

void generateEmergencyVehicle(Vehicle* vehicles[], int& vehicleCount, const Texture textures[], int maxVehicles, Direction dir) {
    if (vehicleCount >= maxVehicles) return;

    int laneIdx = 0; // Emergency vehicles always lane 0
    float startX = positions[dir].x[laneIdx];
    float startY = positions[dir].y[laneIdx];

    const float emergencyVehicleSpeedRange[] = {3.5f, 8.0f};
    float speed = emergencyVehicleSpeedRange[0] + static_cast<float>(rand()) / (RAND_MAX / (emergencyVehicleSpeedRange[1] - emergencyVehicleSpeedRange[0]));

    Vehicle* newVehicle = new Vehicle(
        textures[dir * 5 + 4],
        startX, startY,
        speed,
        laneIdx,
        dir,
        EmergencyVehicle
    );

    string laneName = getLaneName(dir, laneIdx);
    Banker(newVehicle, laneName);
}


void cleanupPipes() {
    const char* pipes[] = {
        "/tmp/smarttraffix_to_challangen",
        "/tmp/challangen_to_userportal",
        "/tmp/stripepayment_to_challangen"
    };

    for (const char* pipeName : pipes) {
        unlink(pipeName);
    }
}

void signalHandler(int signum) {
    cout << "\nInterrupt signal (" << signum << ") received. Cleaning up pipes..." << endl;
    cleanupPipes();
    exit(signum);
}



int main() 
{
int simulatedHour = 0;
int simulatedMinute = 0;
int simulatedDay = 1; 

    RenderWindow window(VideoMode(1700, 800), "Traffic Simulation");
    window.setFramerateLimit(60);

    RectangleShape analyticsLayer(Vector2f(300.f, window.getSize().y));
    analyticsLayer.setPosition(1400.f, 0.f);
    analyticsLayer.setFillColor(Color(50, 50, 50));

    if (!vehicleLogFile.is_open()) 
    {
        cerr << "Error: Unable to open vehicle log file." << endl;
        return -1;
    }

    createNamedPipe("/tmp/smarttraffix_to_challangen");
    createNamedPipe("/tmp/challangen_to_userportal");
    createNamedPipe("/tmp/stripepayment_to_challangen");

    int challanPipeFd = open("/tmp/smarttraffix_to_challangen", O_WRONLY);
    if (challanPipeFd == -1) {
        perror("open challan_pipe for writing");
        return EXIT_FAILURE;
    }

    int challanCounter = 1;

    const int maxVehicles = 1000;
    Vehicle* vehicles[maxVehicles] = {nullptr};
    vector<string> laneNames = {
        "east_lane0","east_lane1",
        "west_lane0","west_lane1",
        "north_lane0","north_lane1",
        "south_lane0","south_lane1"
    };

    for (auto& ln : laneNames) 
    {
        resourceState.available[ln] = 10;
        resourceState.allocated[ln] = 0;
        resourceState.need[ln] = 10; 
    }

    thread monitorThread([&]()
    {
        monitorVehicleSpeeds(vehicles, vehiclesMutex, challanCounter, challanPipeFd);
    });

    Texture backgroundTexture;
    if (!backgroundTexture.loadFromFile("assets/images/Map.png"))
    {
        cerr << "Error: Unable to load background texture." << endl;
        return -1;
    }
    
    Sprite backgroundSprite(backgroundTexture);
    backgroundSprite.setScale(1.85, 1);

    Texture textures[20]; 
    const char* directions[] = {"right", "down", "left", "up"};
    const char* vehicleTypes[] = {"car", "bus", "truck", "bike", "emergency"};
    int textureIndex = 0;

    for (int i = 0; i < 4; ++i) 
    {
        for (int j = 0; j < 5; ++j)
         {
            string path = "assets/images/" + string(directions[i]) + "/" + string(vehicleTypes[j]) + ".png";
            if (!textures[textureIndex].loadFromFile(path))
             {
                cerr << "Error: Unable to load texture: " << path << endl;
                return -1;
            }
            textureIndex++;
        }
    }

    Clock lightClock, heavyClock, emergencyClock;
    Clock northClock, southClock, eastClock, westClock;
    Clock northEmergencyClock, southEmergencyClock, eastEmergencyClock, westEmergencyClock;
    Clock deltaTimeClock; 

    map<Direction, TrafficLight> trafficLights = 
    {
        {Right, TrafficLight(270, 100)},
        {Down, TrafficLight(980, 100)},
        {Left, TrafficLight(980, 520)},
        {Up, TrafficLight(270, 520)}
    };

    for (auto& [direction, light] : trafficLights)
     {
        light.setState(TrafficLight::RED);
    }

    while (window.isOpen()) 
    {
        Event event;
        while (window.pollEvent(event))
         {
            if (event.type == Event::Closed) {
                window.close();
            }
        }

        if (eastClock.getElapsedTime().asSeconds() > 1.5f) {
            runInThread([&]() 
            {
                generateLightVehicle(vehicles, vehicleCount, textures, maxVehicles, Direction::Right);
                analytics.totalLightVehicles++;
            });
            
            eastClock.restart();
        }

        if (eastEmergencyClock.getElapsedTime().asSeconds() > 20.0f) 
        {
            runInThread([&]()
             {
                if (rand() % 100 < 10) 
                {
                    generateEmergencyVehicle(vehicles, vehicleCount, textures, maxVehicles, Direction::Right);
                    analytics.totalEmergencyVehicles++;
                }
            });
            eastEmergencyClock.restart();
        }

        if (westClock.getElapsedTime().asSeconds() > 2.0f) 
        {
            runInThread([&]() 
            {
                generateLightVehicle(vehicles, vehicleCount, textures, maxVehicles, Direction::Left);
                analytics.totalLightVehicles++;
            });
            westClock.restart();
        }

        if (westEmergencyClock.getElapsedTime().asSeconds() > 20.0f) 
        {
            runInThread([&]() {
                if (rand() % 100 < 80) 
                {
                    generateEmergencyVehicle(vehicles, vehicleCount, textures, maxVehicles, Direction::Left);
                    analytics.totalEmergencyVehicles++;
                }
            });
            westEmergencyClock.restart();
        }

        if (northClock.getElapsedTime().asSeconds() > 1.0f)
        {
            runInThread([&]() {
                generateLightVehicle(vehicles, vehicleCount, textures, maxVehicles, Direction::Up);
                analytics.totalLightVehicles++;
            });
            northClock.restart();
        }

        if (northEmergencyClock.getElapsedTime().asSeconds() > 15.0f) 
        {
            runInThread([&]() {
                if (rand() % 100 < 80) {
                    generateEmergencyVehicle(vehicles, vehicleCount, textures, maxVehicles, Direction::Up);
                    analytics.totalEmergencyVehicles++;
                }
            });
            northEmergencyClock.restart();
        }

        if (southClock.getElapsedTime().asSeconds() > 2.0f)
         {
            runInThread([&]() {
                generateLightVehicle(vehicles, vehicleCount, textures, maxVehicles, Direction::Down);
                analytics.totalLightVehicles++;
            });
            southClock.restart();
        }

        if (southEmergencyClock.getElapsedTime().asSeconds() > 15.0f) 
        {
            runInThread([&]() {
                if (rand() % 100 < 5) 
                {
                    generateEmergencyVehicle(vehicles, vehicleCount, textures, maxVehicles, Direction::Down);
                    analytics.totalEmergencyVehicles++;
                }
            });
            southEmergencyClock.restart();
        }

updateSimulationTime(simulationClock, simulatedHour, simulatedMinute, simulatedDay);

if (!isPeakHours(simulatedHour, simulatedMinute) && heavyClock.getElapsedTime().asSeconds() > 15.0f)
 {
    runInThread([&]() 
    {
        generateHeavyVehicle(vehicles, vehicleCount, textures, maxVehicles, static_cast<Direction>(rand() % 4));
        lock_guard<mutex> lock(analyticsMutex);
        analytics.totalHeavyVehicles++;
    });
    
    heavyClock.restart();
}

        processLaneQueues(vehicles, vehicleCount, maxVehicles);
        
        float deltaTime = deltaTimeClock.restart().asSeconds();

        window.clear();
        window.draw(backgroundSprite); 

        Font font;
        if (!font.loadFromFile("assets/fonts/arial_narrow_7.ttf"))
         {
            cerr << "Error: Unable to load font." << endl;
            return -1;
        }
        
	Text timeDateDisplay;
	timeDateDisplay.setFont(font);
	timeDateDisplay.setCharacterSize(24);
	timeDateDisplay.setFillColor(Color::White);


	stringstream ss;
	ss << "Day " << simulatedDay << " Time: " 
	   << setw(2) << setfill('0') << simulatedHour << ":"
	   << setw(2) << setfill('0') << simulatedMinute;

	timeDateDisplay.setString(ss.str());
	timeDateDisplay.setPosition(1000, 0); // Top-left corner
	window.draw(timeDateDisplay);


        updateVehiclesThreaded(vehicles, vehicleCount, deltaTime, trafficLights);

	updateTrafficLights(trafficLights, deltaTime, vehicles, vehicleCount, laneQueues);


        for (auto& [direction, light] : trafficLights) 
        {
            light.draw(window); 
        }

        drawVehicles(window, vehicles, vehicleCount, deltaTime, trafficLights);
        window.draw(analyticsLayer);
        
        displayAnalytics(window, analytics);
        
        window.display();
        
        if (simulationClock.getElapsedTime().asSeconds() >= 300) { 
    		window.close();
}
    }

    vehicleLogFile.close();
    close(challanPipeFd);
    monitoringActive = false;  
    monitorThread.join();     
    for (int i = 0; i < vehicleCount; i++)
     {
        delete vehicles[i];
    }

    return 0;
}


