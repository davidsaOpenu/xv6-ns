#define INPUT_BUF 128 //Maximum length of possible input.
#define HIST_BUF 5 //Size of history buffer (how many commands it can remember).
#define C(x)  ((x)-'@')  // Control-x

//The special key codes for qemu will be defined here     
#define HOME            0xE0
#define END             0xE1
#define ARROW_UP        0xE2
#define ARROW_DOWN      0xE3
#define ARROW_LEFT      0xE4
#define ARROW_RIGHT     0xE5
#define PAGEUP          0xE6
#define PAGEDOWN        0xE7
#define INSERT          0xE8
#define DELETE          0xE9

//The special key codes for qemu-nox will be defined here
#define ARROW_UP_NOX    'A'
#define ARROW_DOWN_NOX  'B'
#define ARROW_RIGHT_NOX 'C'
#define ARROW_LEFT_NOX  'D'
#define END_NOX         'F'
#define HOME_NOX        'H'
#define INSERT_NOX      '2'
#define DELETE_NOX      '3'
#define PAGEUP_NOX      '5'
#define PAGEDOWN_NOX    '6'
