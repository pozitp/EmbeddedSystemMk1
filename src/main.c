#include "main.h"
#include "tm1637.h"

void initKeyboard();
void scanKeyboard();
extern const uint8_t digit_codes[];

volatile uint32_t tickCount;
uint32_t last_display_update;
uint16_t counter;
char lastKey;
uint32_t lastScanTime;


void osSystickHandler(void) {
  tickCount++;
}


void initGPIO() {
  // Включаем тактирование GPIOA и GPIOB
  RCC->AHBENR |= RCC_AHBENR_GPIOAEN | RCC_AHBENR_GPIOBEN;

  // Настраиваем PA5 как выход
  GPIOA->MODER = (GPIOA->MODER & ~(3 << 10)) | (1 << 10);
  GPIOA->OTYPER &= ~(1 << 5);
  GPIOA->OSPEEDR |= (1 << 10);
}

void initUSART2() {
  // Включаем тактирование USART2
  RCC->APB1ENR |= RCC_APB1ENR_USART2EN;

  // Настраиваем PA2 и PA3 в альтернативный режим
  GPIOA->MODER = (GPIOA->MODER & ~(0xF << 4)) | (0xA << 4);
  GPIOA->AFR[0] = (GPIOA->AFR[0] & ~(0xFF << 8)) | (1 << 8) | (1 << 12);

  // Настраиваем USART2
  USART2->BRR = 417; // 48MHz/115200
  USART2->CR1 = USART_CR1_TE | USART_CR1_UE;
}

void initSysTick() {
  SysTick->LOAD = 47999; // 1ms при 48MHz
  SysTick->VAL = 0;
  SysTick->CTRL = (1 << 2) | (1 << 1) | (1 << 0);
}

int _write(int file, uint8_t *ptr, int len) {
  for (int i = 0; i < len; i++) {
    while (!(USART2->ISR & USART_ISR_TXE));
    USART2->TDR = ptr[i];
  }
  return len;
}

void checkTickCount() {
  if ((tickCount % 2000) == 0) {
    GPIOA->ODR ^= (1 << 5); // Toggle LED
    printf("tickCount = %d!\n", tickCount++);
  }
}

int main(void) {
  char password[4];
  char code[4];
  uint8_t length = 0;
  uint8_t locked = 0;
  char previousKey = '\0';

  initGPIO();
  initUSART2();
  initSysTick();
  initKeyboard();
  tm1637_init();

  GPIOA->MODER = (GPIOA->MODER & ~((3U << 30) | (3U << 18)))
                 | (1U << 30) | (1U << 18);
  GPIOA->ODR &= ~((1U << 15) | (1U << 9));
  GPIOA->ODR |= (1U << 9);
  GPIOB->BSRR = (1U << 7) | (1U << 6) | (1U << 3);
  GPIOA->BSRR = (1U << 10);
  tm1637_clear();

  printf("Hello, %s!\n", "Wokwi Simulation");
  printf("Safe open: enter 4 digits and press # to lock. * clears input.\n");

  GPIOA->ODR |= (1 << 5); // Включаем LED

  while (1) {
    checkTickCount();
    scanKeyboard();

    if (lastKey == previousKey) continue;
    previousKey = lastKey;

    if (lastKey == '*') {
      length = 0;
      tm1637_clear();
    } else if (lastKey == '#') {
      if (length != 4) {
        printf("Enter 4 digits.\n");
        continue;
      }

      if (!locked) {
        memcpy(password, code, 4);
        locked = 1;
        GPIOA->ODR &= ~(1U << 9);
        GPIOA->ODR |= (1U << 15);
        printf("Safe locked.\n");
      } else if (memcmp(code, password, 4) == 0) {
        locked = 0;
        GPIOA->ODR &= ~(1U << 15);
        GPIOA->ODR |= (1U << 9);
        printf("Safe open: enter a new code and press # to lock.\n");
      } else {
        printf("Wrong code. Safe locked.\n");
      }

      length = 0;
      tm1637_clear();
    } else if (lastKey >= '0' && lastKey <= '9' && length < 4) {
      code[length] = lastKey;
      tm1637_display_digit(length, digit_codes[lastKey - '0']);
      length++;
    }
  }

  return 0;
}
