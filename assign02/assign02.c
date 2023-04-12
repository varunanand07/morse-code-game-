#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "pico/stdlib.h"
#include "hardware/pio.h"
#include "hardware/clocks.h"
#include "assign02.pio.h"
#include "hardware/watchdog.h"

#define IS_RGBW true        // Will use RGBW format
#define NUM_PIXELS 1        // There is 1 WS2812 device in the chain
#define WS2812_PIN 28       // The GPIO pin that the WS2812 connected to

int rgbColour = 4; // colour for RGB LED
char input[6];
char morseCopy[6];
int j = 0;
int level = 0;
int lives = 3;
int wins = 0;
int sel = 2;
int currentCharacter = 0;
bool inputFinished = false;
int currentCharacterLength = 0;

/**
 * @brief Must declare the main assembly entry point before use.
 * 
 * @return returns nothing
 */
void main_asm();

 /**
 * @brief Initialise a GPIO pin – see SDK for detail on gpio_init()
 * 
 * @param pin contains the number of the pin 
 * 
 * @return returns nothing
 */
void asm_gpio_init(uint pin) {
    gpio_init(pin);
}

/**
* @brief Set direction of a GPIO pin – see SDK for detail on gpio_set_dir()
* 
* @param pin contains the number of the pin 
* 
* @param out boolean determines whether the variable is an input or the output
* 
* @return returns nothing
*/
void asm_gpio_set_dir(uint pin, bool out) {
    gpio_set_dir(pin, out);
}

/**
* @brief Enable falling-edge & rising-edge interrupt – see SDK for detail on gpio_set_irq_enabled()
* 
* @param pin contains the number of the pin
* 
* @return returns nothing
*/
void asm_gpio_set_irq(uint pin) {
    gpio_set_irq_enabled(pin, GPIO_IRQ_EDGE_FALL, true);
    gpio_set_irq_enabled(pin, GPIO_IRQ_EDGE_RISE, true);
}

/**
* @brief Wrapper function used to call the underlying PIO
*        function that pushes the 32-bit RGB colour value
*        out to the LED serially using the PIO0 block. The
*        function does not return until all of the data has
*        been written out.
* 
* @param pixel_grb The 32-bit colour value generated by urgb_u32()
*/
static inline void put_pixel(uint32_t pixel_grb) {
    pio_sm_put_blocking(pio0, 0, pixel_grb << 8u);
}

/**
* @brief Function to generate an unsigned 32-bit composit GRB
*        value by combining the individual 8-bit paramaters for
*        red, green and blue together in the right order.
* 
* @param r     The 8-bit intensity value for the red component
* 
* @param g     The 8-bit intensity value for the green component
* 
* @param b     The 8-bit intensity value for the blue component
* 
* @return uint32_t Returns the resulting composit 32-bit RGB value
*/
static inline uint32_t urgb_u32(uint8_t r, uint8_t g, uint8_t b) {
    return  ((uint32_t) (r) << 8)  |
            ((uint32_t) (g) << 16) |
            (uint32_t) (b);
}

/**
* @brief RGB LED colours indicating the stages of the games
*        Sets the colour of the RGB LED on the MAKER-PI-PICO
*        based on the players current stage/progress in the game.
*        using one of the RP2040 built-in PIO controllers.
* 
* @param rgbColour number indicating the colour we want to set the RGB LED to 
* 
* @return returns nothing
*/
void setRgbStatus(int rgbColour) {
    if(rgbColour == 4) // If game is not in progress set RGB LED to blue
        put_pixel(urgb_u32(0x00, 0x00, 0xFF));
    else {
        // Game is in progress...
        if(rgbColour == 3)
            put_pixel(urgb_u32(0x00, 0x7F, 0x00)); // If game has started and player has 3 lives set the RGB LED to green
        else if(rgbColour == 2)
            put_pixel(urgb_u32(0xFF, 0xFF, 0x00)); // If game has started and player has 2 lives set the RGB LED to yellow
        else if(rgbColour == 1)
            put_pixel(urgb_u32(0xFC, 0x74, 0x05)); // If game has started and player has 1 lives set the RGB LED to orange
        else if(rgbColour == 0)
            put_pixel(urgb_u32(0x7F, 0x00, 0x00)); // If game has started and player has 0 lives set the RGB LED to red, this means it is game over
    }
}

/**
* @brief  reloads the watchdog counter with the amount of time set in watchdog_enable
* 
* @return returns nothing
*/
void watchdog_update();

/**
* @brief enables the watchdog
* 
* @param delay_ms defines how much time the program has to wait before it resets
* 
* @param pause_on_debug enables the watchdog
* 
* @return
*/
void watchdog_enable(uint32_t delay_ms, bool pause_on_debug);

/**
 * @brief Character array for the alphanumeric digits & letters
 * 
 * @return
 */
char alphanumericCharacters[] = {
    // Alphanumeric letters A - Z
    'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H', 'I', 'J', 'K', 'L', 'M', 'N', 'O', 'P', 'Q', 'R', 'S', 'T', 'U', 'V', 'W', 'X', 'Y', 'Z',
    // Alphanumeric digits 0 - 9
    '0', '1', '2', '3', '4', '5', '6', '7', '8', '9'};

/**
 * @brief Character array for the morse code digits & letters
 * 
 * @return
 */
char morseCodeCharacters[36][5] = { 
    // Morse code for the letters A - Z
    ".-", "-...", "-.-.", "-..", ".", "..-.", "--.", "....",
    "..", ".---", "-.-", ".-..", "--", "-.", "---", ".--.",
    "--.-", ".-.", "...", "-", "..-", "...-", ".--", "-..-",
    "-.--", "--..",
    // Morse code for the digits 0-9
    "-----", ".----", "..---", "...--", "....-", ".....",
    "-....", "--...", "---..", "----."};

/**
* @brief prints the welcome message banner
* 
* @return returns nothing
*/
void welcome_message_banner() {
  printf("\n+--------------------------------------------------------+\n");
  printf("|              ASSIGNMENT #02      Group 11              |\n");
  printf("+--------------------------------------------------------+\n");
  printf("|    +       +  +------+  +------+   +----+  +------+    |\n");
  printf("|    | \\   / |  |      |  |       +  |       |           |\n");
  printf("|    |   +   |  |      |  |------+   +----+  |----+      |\n");
  printf("|    |       |  |      |  |     \\         |  |           |\n");
  printf("|    +       +  +------+  +      +   +----+  +------+    |\n");
  printf("|                                                        |\n");
  printf("|         +------+  +------+  +----+    +------+         |\n");
  printf("|         |         |      |  |     \\   |                |\n");
  printf("|         |         |      |  |      +  |----+           |\n");
  printf("|         |         |      |  |     /   |                |\n");
  printf("|         +------+  +------+  +----+    +------+         |\n");
  printf("|                                                        |\n");
  printf("|          +------+    ^    +       +  +------+          |\n");
  printf("|          |         /   \\  | \\   / |  |                 |\n");
  printf("|          |   +--|  |---|  |   +   |  |----+            |\n");
  printf("|          |      |  |   |  |       |  |                 |\n");;
  printf("|          +------+  +   +  +       +  +------+          |\n");
  printf("+--------------------------------------------------------+\n");
  printf("|       TO BEGIN, PRESS ON GP21 TO ENTER SEQUENCE        |\n");
  printf("|         \".----\" - LEVEL 01 - CHARACTERS (EASY)         |\n");
  printf("|         \"..---\" - LEVEL 02 - CHARACTERS (HARD)         |\n");
  printf("+--------------------------------------------------------+\n");
  printf("Rules:\n");
  printf("1. Enter the character displayed in morse code.\n");
  printf("2. If you get the character correct you win a life (max number of lives: 3).\n");
  printf("3. Otherwise you lose a life. The LED will show you how many lives you have.\n");
  printf("4. If you do not enter a character for a period of 9 seconds, the game will reset.\n");
  printf("5. If you end up with zero lives, you lose.\n");
  printf("\nCHOOSE A LEVEL FROM THE ONES SHOWN ABOVE: ");
}

/**
* @brief copies the Morse Code to a specified index
* 
* @param morseIndex number assigned to the Morse Code value
* 
* @return returns nothing
*/
void copyMorseCode(int morseIndex) {
    int length = 0;
    for (int index = 0; index < 5 && (morseCodeCharacters[morseIndex][index] == '.' ||
    morseCodeCharacters[morseIndex][index] == '-'); index++) {
        morseCopy[index] = morseCodeCharacters[morseIndex][index];
        length++;
    }
    morseCopy[length] = '\0';
}

/**
* @brief resets the Morse Code array
* 
* @return returns nothing
*/
void resetMorseCodeArray() {
    for (int index = 0; index < 6; index++)
        morseCopy[index] = 0;
}

/**
* @brief resets variables used in the game
* 
* @return returns nothing
*/
void resetValues() {
    lives = 3;
    level = 0;
    wins = 0;
    currentCharacter = 0;
    currentCharacterLength = 0;
    //inputFinished = false;
}

/**
* @brief defines the case when player wins the game
* 
* @return because its a boolean function it returns either true or false
*/
bool playerWinsGame() {
    if (wins == 10) {
        printf("Woo hoo! You win!\n");
        resetValues();
        return true;
    }
    else
        return false;
}

/**
* @brief defines the case when playes looses the game
* 
* @return because its a boolean function it returns either true or false
*/
bool playerLosesGame() {
    if (lives == 0) {
        printf("Game over... You lose...\n");
        resetValues();
        return true;
    }
    else
        return false;
}

/**
* @brief defines the current input of the player
* 
* @return returns nothing
*/
void addSymbolToAnswer() {
    // if player entered "."
    if(sel == 0) {
        printf(".");
        input[j++] = '.';
        watchdog_update();
        sel = 2;
    }
    // if player entered "-"
    else if(sel == 1) {
        printf("-");
        input[j++] = '-';
        watchdog_update();
        sel = 2;
    }
    // if no input was given during an active game (not during the level decision)
    else if(level != 0 && sel == 2) {
        printf(" \n");
        input[5] = 1;
        input[j] = '\0';
        inputFinished = true;
    }
    if (j == currentCharacterLength) {
        input[currentCharacterLength] = '\0';
        printf("\n");
        inputFinished = true;   
    }
    if (j == 5 && level == 0 && input[0] != '\0') {
        input[5] = '\0';
        printf("\n");
        inputFinished = true;
    }
}

/**
* @brief checks if the player either lost or won the current game
* 
* @param indexFromInput the index of the matching Morse Code for the input that the player entered
* 
* @return because its a boolean function it returns either true or false
*/
bool checkForWinOrLossInCurrentRound(int indexFromInput) {
    if (indexFromInput == currentCharacter)
        return true;
    else
        return false;
}

/**
* @brief decreases the number of lives and prints it
* 
* @return returns nothing
*/
void loseLife() {
    if (lives != 0)
        lives--;
    printf("You have %d lives left.\n\n", lives);
}

/**
* @brief increases the number of lives and prints it
* 
* @return returns nothing
*/
void winLife() {
    if (lives != 3)
        lives++;
    printf("You have %d lives left.\n\n", lives);
}

/**
* @brief resets the array thats used to gather the player's input
*  
* @return returns nothing
*/
void resetAnswer() {
    for (int index = 0; index < 6; index++)
        input[index] = 0;
    j = 0;
    inputFinished = false;
    //clear_buffer();
}

/**
* @brief updates the number of wins and checks whether the player has 5 wins on the current level and if he, it goes to the next level
* 
* @return returns nothing
*/
void updateNumberOfWins() {
    wins++;
    if (wins == 5) {
        printf("Congratulations! You passed to level 02.\n");
        printf("In level 02, you will not be shown the Morse codes of characters.\n");
        level++;
    }
}

/**
* @brief handles a win in the current round
* 
* @return returns nothing
*/
void handleWinInCurrentRound() {
    printf("Player entered the correct answer!\n");
    winLife();
    updateNumberOfWins();
    resetAnswer();
    setRgbStatus(lives);
}

/**
* @brief handles a lose in the current round
* 
* @return returns nothing
*/
void handleLoseInCurrentRound() {
    if (input[5] != 1)
        printf("Player entered the wrong answer...\n");
    else
        printf("Player didn't enter an answer...\n");
    loseLife();
    resetAnswer();
    setRgbStatus(lives);
}

/**
* @brief shows the selection of levels to the player, read player's input and chooses a level
* 
* @return if it returns 0 your entered a wrong input, if it returns 1 or 2 it chosses level 1 or 2
*/
int initialLevelSelection() {
    if (strcmp(input, ".----") == 0) {
        watchdog_update();
        printf("\nYou chose level 01.\n");
        setRgbStatus(lives);
        resetAnswer();
        //clear_buffer();
        return 1;
    }
    if (strcmp(input, "..---") == 0) {
        watchdog_update();
        printf("\nYou chose level 02.\n");
        //clear_buffer();
        wins = 5;
        setRgbStatus(lives);
        resetAnswer();
        return 2;
    }
    if (inputFinished) {
        printf("\nWrong input was entered.\n");
        resetAnswer();
    }
    return 0;
}

/**
* @brief prints player's input
* 
* @return returns nothing
*/
void printPlayerInput() {
    printf("Your input was: ");
    for(int index = 0; index < currentCharacterLength; index++) {
        printf("%c", input[index]);
    }
    printf("\n");
}

/**
 * @brief counts the number of valid characters in the morse code at the specified index
 * 
 * @return the number of elements in the given morse code
 */
int numberOfCharacters(int morseIndex) {
    bool outOfBounds = false;
    int length = 0;
    for (int index = 0; index < 5 && !outOfBounds; index++)
        if (morseCodeCharacters[morseIndex][index] != '.' && morseCodeCharacters[morseIndex][index] != '-')
            outOfBounds = true;
        else
            length++;
    return length;
}

/**
* @brief checks to see if theres an existing Morse Code to correspond to the player's input and then returns the index of the corresponding Morse Code
* 
* @return if it returns -1 it means theres no match or the index 
*/
int linkMorseToCorrespondingCharacter() {
    for(int index = 0; index < 37; index++) {
        copyMorseCode(index);
        int length = 0;
        if (strlen(morseCopy) > strlen(input))
            length = strlen(morseCopy);
        else
            length = strlen(input);
        if (strncmp(input, morseCopy, length) == 0) {
                printf("This corresponds to character \"%c\".\n", alphanumericCharacters[index]);
                resetMorseCodeArray();
               return index;
            }
        resetMorseCodeArray();
        }
        printf("This corresponds to character \"?\".\n");
        return -1;
}

/**
* @brief sets up level 01 for the game
* 
* @return returns nothing
*/
void level01() {
    watchdog_update();
    currentCharacter = rand() % 36; // gens random number in index to test
    printf("Enter the following character in Morse code: %c\n", alphanumericCharacters[currentCharacter]);
    printf("Morse code for character \"%c\": ", alphanumericCharacters[currentCharacter]);
    for(int i = 0; i < 5; i++) {
        printf("%c", morseCodeCharacters[currentCharacter][i]);
    }
    currentCharacterLength = numberOfCharacters(currentCharacter);
    printf("\n");
    j = 0;
}

/**
 * @brief sets up level 02 for the game
 *
 * @return  returns nothing
*/
void level02() {
    watchdog_update();
    currentCharacter = rand() % 36; // gens random number in index to test
    printf("Enter the following character in Morse code: %c\n", alphanumericCharacters[currentCharacter]);
    currentCharacterLength = numberOfCharacters(currentCharacter);
    j = 0;
}

/**
* @brief function that is being called by one of the interrupts when it is triggered
* 
* @param symbol an integer that corresponds to one of the symbols a player can add to his amswer
* 
* @return returns nothing
*/
void Gameflow(int symbol) {
    watchdog_enable(0x2328, 1);
    if (lives != 0) {
        sel = symbol;
        addSymbolToAnswer();
        if (level == 0) {
            level = initialLevelSelection();
            if (level == 1)
                    level01();
            else if (level == 2)
                    level02();
        }
        else {
            if (inputFinished) {
                printPlayerInput();
                int indexFromInput = linkMorseToCorrespondingCharacter();
                if (checkForWinOrLossInCurrentRound(indexFromInput))
                    handleWinInCurrentRound();
                else
                    handleLoseInCurrentRound();
                if (!playerWinsGame() && !playerLosesGame()) {
                    if (level == 1)
                        level01();
                    else
                        level02();
                }
            }
        }
    }
}

/**
* @brief Main entry point for the code - simply calls the main assembly function.
* 
* @return returns the default value
*/
int main() {

    // Initialise all STDIO as we will be using the GPIOs
    stdio_init_all();

    // Initialise the PIO interface with the RGB code in the assign02.pio file containing the ws2812 code
    PIO pio = pio0;
    uint offset = pio_add_program(pio, &ws2812_program);
    ws2812_program_init(pio, 0, offset, WS2812_PIN, 800000, IS_RGBW);

    // Game is not in progress so set the RGB LED to blue - need to edit intensity
    setRgbStatus(rgbColour);
    sleep_ms(500);
    welcome_message_banner();

    main_asm();

    return(0);
}
