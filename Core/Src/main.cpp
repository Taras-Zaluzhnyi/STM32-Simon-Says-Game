/*
 * @brief STM32 Simon Says Game
 * A simple memory game - Simon Says. In which you need to repeat the blinking of LED after Simon.
 * Implemented on the STM32F411 Black Pill using C++ and HAL
 * Author: Taras Zaluzhnyi
 */
#include "main.h"
#include <random>

constexpr uint8_t MAX_LEVEL = 5; // Number of levels
uint32_t sequence[MAX_LEVEL];    // LED sequence array for gaming situation
uint8_t current_level;           // Tracks player progress (0 to MAX_LEVEL-1)

// Game timing constants (in ms)
constexpr uint32_t GAME_SPEED_MS = 500;
constexpr uint32_t ERROR_BLINK_MS = 200;
constexpr uint32_t WIN_ANIMATION_MS = 100;

// Auxiliary variables for creating a random blinking sequence
std::mt19937 generator;
std::uniform_int_distribution<uint32_t> distrib(0, 3);

//// Mapping logical indices (0-3) to physical GPIO pins on the board
uint16_t led_pins[] = { GPIO_PIN_3, GPIO_PIN_4, GPIO_PIN_5, GPIO_PIN_6 };
uint16_t button_pins[] = { GPIO_PIN_3, GPIO_PIN_4, GPIO_PIN_5, GPIO_PIN_6 };
constexpr uint8_t BUTTON_COUNT = 4;
constexpr uint8_t LED_COUNT = 4;

/* An enumeration that helps determine the stage of the game and control the stage in the state machine */
enum GameState {
	IDLE,        // Start of the game
	SIMON_SAYS,  // Demonstrate the sequence to the player
	PLAYER_SAYS, // Player repeats the sequence
	GAME_OVER,   // End of the game: Loss
	WIN          // End of the game: Victory
};

void SystemClock_Config(void);
static void MX_GPIO_Init(void);

/**
 * @brief  Checks if a button has been pressed
 * @return Array index of the pressed button
 */
int8_t GetPressedButtonIndex() {
	for (int i = 0; i < BUTTON_COUNT; ++i) {
		if (HAL_GPIO_ReadPin(GPIOB, button_pins[i]) == 0) {
			return i;
		}
	}
	return -1;
}
/**
 * @brief  Flashes all LEDs when losing
 * @return None
 */
void ToggleLEDsForGameOver() {
	for (int i = 0; i < 4; ++i) {
		HAL_GPIO_TogglePin(GPIOA,
				led_pins[0] | led_pins[1] | led_pins[2] | led_pins[3]);
		HAL_Delay(ERROR_BLINK_MS);
	}
}
/**
 * @brief  Launches "Running Light" when winning
 * @return None
 */
void RunningLightForWin() {
	for (int j = 0; j < 4; ++j) {
		for (int i = 0; i < LED_COUNT; ++i) {
			HAL_GPIO_WritePin(GPIOA, led_pins[i], GPIO_PIN_SET);
			HAL_Delay(WIN_ANIMATION_MS);
			HAL_GPIO_WritePin(GPIOA, led_pins[i], GPIO_PIN_RESET);
		}
	}
}
/**
 * @brief  The application entry point.
 * @return int
 */
int main(void) {
	/* MCU Configuration--------------------------------------------------------*/
	/* Reset of all peripherals, Initializes the Flash interface and the Systick. */
	HAL_Init();

	/* Configure the system clock */
	SystemClock_Config();

	/* Initialize all configured peripherals */
	MX_GPIO_Init();

	/* Determining the initial state of the game */
	GameState state = IDLE;

	/* Implementing the gameplay using a state machine method*/
	while (1) {
		switch (state) {
		// Game start: expect the player to press the Start button
		case IDLE:
			if (HAL_GPIO_ReadPin(START_GPIO_Port, START_Pin) == 0) {
				generator.seed(HAL_GetTick());
				current_level = 0;
				state = SIMON_SAYS;
			}
			break;

			/* Demonstrate the sequence to the player */
		case SIMON_SAYS:
			/* With each additional level, we add one new blink. */
			sequence[current_level] = distrib(generator);
			/* LED flashes alternately depending on the level */
			for (int i = 0; i <= current_level; ++i) {
				HAL_GPIO_WritePin(GPIOA, led_pins[sequence[i]], GPIO_PIN_SET);
				HAL_Delay(GAME_SPEED_MS);
				HAL_GPIO_WritePin(GPIOA, led_pins[sequence[i]], GPIO_PIN_RESET);
				HAL_Delay(GAME_SPEED_MS);
				state = PLAYER_SAYS;
			}
			break;

			/* Player repeats the sequence */
		case PLAYER_SAYS:
			for (int i = 0; i <= current_level; ++i) {
				bool flag = false;
				while (!flag) {
					/* Wait for a button to be pressed and read which button was pressed.
					 When pressed, the LED flashes */
					int8_t index = GetPressedButtonIndex();
					if (index >= 0) {
						HAL_GPIO_WritePin(GPIOA, led_pins[index], GPIO_PIN_SET);
						HAL_Delay(GAME_SPEED_MS);
						/* Waiting for the button to be released */
						while (HAL_GPIO_ReadPin(GPIOB, button_pins[index]) == 0) {
						}
						HAL_GPIO_WritePin(GPIOA, led_pins[index],
								GPIO_PIN_RESET);
						HAL_Delay(GAME_SPEED_MS);
						/* Check if the player's input matches the expected sequence
						 If so, exit the loop and move on to the next LED */
						if (static_cast<uint32_t>(index) == sequence[i]) {
							flag = true;
						}
						/* If not, the player loses */
						else {
							state = GAME_OVER;
							break;
						}
					}
				}
				/* Checking if the player has not lost after pressing the button */
				if (state != PLAYER_SAYS) {
					break;
				}
			}
			/* If all the LEDs are pressed correctly, increase the level,
			 or count the victory if the maximum level is reached */
			if (state == PLAYER_SAYS) {
				if (current_level == MAX_LEVEL - 1) {
					state = WIN;
				} else {
					++current_level;
					state = SIMON_SAYS;
				}
			}
			break;

			/* Player loss */
		case GAME_OVER:
			ToggleLEDsForGameOver();
			state = IDLE;
			break;

			/* Player win */
		case WIN:
			RunningLightForWin();
			state = IDLE;
			break;
		}
	}
}

/**
 * @brief System Clock Configuration
 * @return None
 */
void SystemClock_Config(void) {
	RCC_OscInitTypeDef RCC_OscInitStruct = { 0 };
	RCC_ClkInitTypeDef RCC_ClkInitStruct = { 0 };

	/** Configure the main internal regulator output voltage
	 */
	__HAL_RCC_PWR_CLK_ENABLE();
	__HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE1);

	/** Initializes the RCC Oscillators according to the specified parameters
	 * in the RCC_OscInitTypeDef structure.
	 */
	RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
	RCC_OscInitStruct.HSEState = RCC_HSE_ON;
	RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
	RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
	RCC_OscInitStruct.PLL.PLLM = 25;
	RCC_OscInitStruct.PLL.PLLN = 168;
	RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV2;
	RCC_OscInitStruct.PLL.PLLQ = 4;
	if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK) {
		Error_Handler();
	}

	/** Initializes the CPU, AHB and APB buses clocks
	 */
	RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_SYSCLK
			| RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2;
	RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
	RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
	RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV2;
	RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

	if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_2) != HAL_OK) {
		Error_Handler();
	}
}

/**
 * @brief GPIO Initialization Function
 * @param None
 * @return None
 */
static void MX_GPIO_Init(void) {
	GPIO_InitTypeDef GPIO_InitStruct = { 0 };

	/* GPIO Ports Clock Enable */
	__HAL_RCC_GPIOH_CLK_ENABLE();
	__HAL_RCC_GPIOA_CLK_ENABLE();
	__HAL_RCC_GPIOB_CLK_ENABLE();

	/*Configure GPIO pin Output Level */
	HAL_GPIO_WritePin(GPIOA, GPIO_PIN_3 | GPIO_PIN_4 | GPIO_PIN_5 | GPIO_PIN_6,
			GPIO_PIN_RESET);

	/*Configure GPIO pin : START_Pin */
	GPIO_InitStruct.Pin = START_Pin;
	GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
	GPIO_InitStruct.Pull = GPIO_PULLUP;
	HAL_GPIO_Init(START_GPIO_Port, &GPIO_InitStruct);

	/*Configure GPIO pins : PA3 PA4 PA5 PA6 */
	GPIO_InitStruct.Pin = GPIO_PIN_3 | GPIO_PIN_4 | GPIO_PIN_5 | GPIO_PIN_6;
	GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
	GPIO_InitStruct.Pull = GPIO_NOPULL;
	GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
	HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

	/*Configure GPIO pins : PB3 PB4 PB5 PB6 */
	GPIO_InitStruct.Pin = GPIO_PIN_3 | GPIO_PIN_4 | GPIO_PIN_5 | GPIO_PIN_6;
	GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
	GPIO_InitStruct.Pull = GPIO_PULLUP;
	HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);
}

/**
 * @brief  This function is executed in case of error occurrence.
 * @return None
 */
void Error_Handler(void) {
	__disable_irq();
	while (1) {
	}
}
#ifdef USE_FULL_ASSERT
/**
  * @brief  Reports the name of the source file and the source line number
  *         where the assert_param error has occurred.
  * @param  file: pointer to the source file name
  * @param  line: assert_param error line source number
  * @return None
  */
void assert_failed(uint8_t *file, uint32_t line) {
}

#endif /* USE_FULL_ASSERT */
