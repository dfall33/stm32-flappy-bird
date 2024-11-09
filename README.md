# Flappy Chip

Flappy Chip is a hardware and software project that recreates the popular mobile game "Flappy Bird" using the STM32F091 microcontroller. The player controls a bird that must navigate through moving vertical obstacles by pressing a button to make the bird jump. The game includes features such as score tracking, high score saving, and graphical display on a TFT screen.

## Features

- **High Score Tracking**: The game saves the highest score to an EEPROM and displays it before the game starts or between rounds.
- **Gameplay Mechanics**: The bird moves forward at a constant rate and is affected by gravity. The player controls the bird’s vertical movement by pressing a push button, attempting to avoid obstacles and the ground.
- **Obstacle Generation**: Vertical barriers appear randomly on the screen with a gap through which the bird must pass. The gap's position is randomly generated at the start of each new barrier.
- **Display**: A 240x320 QVGA TFT display shows the bird, barriers, and background. The current score is displayed on two 8-segment displays.
- **User Input**: The game uses the onboard push button (PA0) to start the game and control the bird.

## Project Objectives

### 1. High Scores: 
The project implements the ability to read and write high score data to and from an EEPROM using the I2C protocol.

- **How it's achieved**: 
  - When the game ends, the current score is compared to the high score stored in EEPROM. If the current score is higher, it is saved to the EEPROM using I2C. 
  - The high score is read and displayed on the 8-segment displays when the game starts or between rounds.

### 2. Game Background and UI:
The project displays the game background and dynamically updates the locations of obstacles using the TFT display and SPI communication.

- **How it's achieved**:
  - The background image is drawn using SPI communication to the TFT display. The bird and barrier images are overlayed on top of the background, simulating motion by updating the bird and obstacle positions.
  - The locations of obstacles are randomly generated using a pseudo-random number generator. Barriers are drawn at these locations, with gaps for the bird to pass through.

### 3. Simulating Motion:
The bird’s motion is simulated with a gravitational effect. The bird's position is updated based on velocity and acceleration, and user input is factored in using a timer-triggered interrupt.

- **How it's achieved**:
  - The bird’s velocity is updated based on a constant acceleration that simulates gravity. When the player presses the push button, the bird’s upward velocity is increased.
  - The bird’s position is updated every timer interrupt, and the new position is written to the TFT display using SPI and DMA.
  - The push button is read within the interrupt, and if pressed, it boosts the bird's velocity to simulate a jump.

### 4. Displaying High Score and Current Score:
The project uses two 8-segment displays to show both the current score and high score during gameplay.

- **How it's achieved**:
  - The current score is displayed on the 8-segment displays and updated every time the player successfully passes an obstacle.
  - The high score is saved to EEPROM and displayed when the game starts or between rounds.
  - A font lookup table is used to convert numeric values into a format compatible with the 8-segment displays.

## Hardware

### Components Used:
- **STM32F091** microcontroller
- **TFT Display**: 240x320 QVGA, rotated sideways, communicates via SPI
- **8-Segment Displays**: Two 8-segment displays for showing the current score and high score
- **User Input**: PA0 push button to control the bird and start the game
- **External Components**: Resistors, capacitors, transistors for powering display segments, 74HC13B decoder for controlling the 8-segment displays
- **EEPROM**: To store the high score via I2C

## Software Implementation

- **SPI**: Used for communication with the TFT display to render the game graphics.
- **I2C**: Used to read from and write to the EEPROM for storing and retrieving the high score.
- **DMA**: Used to efficiently transfer game data to/from the TFT display and EEPROM.
- **Timers and Interrupts**: Handle the bird's movement, update the screen, and read the push button for user input.
- **Game Logic**: The bird’s position is updated based on velocity and acceleration, with input from the button. The game checks for collisions with barriers and the ground, and the score is updated accordingly.

## How to Play

1. **Start the Game**: Press the PA0 push button to begin.
2. **Control the Bird**: Press the button again to make the bird jump. The bird’s position is affected by gravity, and pressing the button makes the bird move upward.
3. **Avoid Obstacles**: Navigate through the gaps in the obstacles by timing your jumps. The bird moves forward at a constant rate.
4. **Score**: Every time you successfully pass an obstacle, your score increases by 1.
5. **Game Over**: The game ends when the bird hits a barrier or falls to the ground.
6. **High Score**: After the game ends, your score is compared to the high score saved in EEPROM. If your score is higher, it is saved as the new high score.

## Future Improvements
- **Add sound effects**: Implement sound effects for bird jumps and collisions.
- **Add more levels or speed increases**: Gradually increase the speed of the obstacles or the bird to make the game more challenging as the player progresses.
- **Customize visuals**: Implement different backgrounds, bird skins, or obstacles for more variety.

Enjoy playing **Flappy Chip**!
