# SmartTraffix: Multithreaded Traffic Management Simulation

## Project Overview
SmartTraffix is a multithreaded traffic management system designed to simulate and optimize traffic flow at a four-way intersection. This system utilizes SFML for visualization and is developed in C++ with POSIX API threads on Ubuntu. It incorporates advanced traffic management techniques including round-robin traffic light control, priority handling for emergency vehicles, and communication between different entities using pipes.

## Key Features

- **Traffic Light Management**: Implements a round-robin scheduling algorithm for traffic lights, with overrides for emergency vehicles.
- **Vehicle Prioritization**: Emergency vehicles are given immediate passage, altering traffic light states to facilitate their movement.
- **Dynamic Vehicle Simulation**: Vehicles arrive from four directions with varying probabilities. Vehicle speeds increase every 5 seconds, and speed violations result in automated challan generation, except for emergency vehicles.
- **Challan System**: Integrates with a challan generator and user portal for handling and payment of fines via a Stripe payment portal.
- **Deadlock Prevention**: Uses the Banker's algorithm to manage vehicle queuing and resource allocation, preventing deadlocks at the intersection.

## System Components

1. **SmartTraffix Core**: Manages traffic lights, vehicle queues, and vehicle prioritization.
2. **Challan Generator**: Monitors vehicle speeds and issues challans for speeding violations.
3. **User Portal**: Allows vehicle owners to view and pay challans.
4. **Stripe Payment Integration**: Handles payment processing for challans.

## Simulation Details

- **Duration**: Each simulation run lasts for 5 minutes, representing 24 hours of traffic flow.
- **Vehicle Types**: Supports regular vehicles, heavy vehicles during non-peak hours, and emergency vehicles with priority.
- **Visualization**: Utilizes SFML to render the traffic system, including animated vehicles and color-coded traffic lights.

## Getting Started

### Prerequisites

- Linux OS (Preferably Ubuntu)
- SFML installed
- C++17 compiler
- POSIX threads support

### Installation

1. Clone the repository:
   ```bash
   git clone https://github.com/mmasabalvi/SmartTraffix-Simulation.git

2. Navigate to the project directory:
   ```bash
   cd SmartTraffix-Simulation
   
3. Compile the project using 4 different terminals:
   ```bash
   g++ Challan_Generator.cpp -o challan -lsfml-graphics -lsfml-window -lsfml-system -pthread -std=c++17
   ./challan
   g++ User_Portal.cpp -o user -lsfml-graphics -lsfml-window -lsfml-system -pthread -std=c++17
   ./user
   g++ Stripe_Payment.cpp -o stripe -lsfml-graphics -lsfml-window -lsfml-system -pthread -std=c++17
   ./stripe
   g++ Smart_Traffix.cpp -o main -lsfml-graphics -lsfml-window -lsfml-system -pthread -std=c++17
   ./main

### Contributions
Contributions are welcome. Please fork the repository and submit pull requests to enhance the simulation or fix bugs. 





