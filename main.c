#include <SDL3/SDL.h>
#define SDL_MAIN_USE_CALLBACKS 1
#include <SDL3/SDL_main.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define BUTTON_COUNT 17
#define INPUT_SIZE 64

struct Button {
    SDL_FRect rect;
    const char *label;
    char action;
};

struct Context {
    SDL_Window *window;
    SDL_Renderer *renderer;

    int width;
    int height;

    const char *title;

    struct Button buttons[BUTTON_COUNT];

    char input[INPUT_SIZE];
    char expression[INPUT_SIZE * 2];
    char result[INPUT_SIZE];

    double stored_value;
    char operation;
    int waiting_for_number;
};

static void set_input(struct Context *ctx, const char *text)
{
    snprintf(ctx->input, sizeof(ctx->input), "%s", text);
}

static void update_expression(struct Context *ctx)
{
    if (ctx->operation != '\0') {
        snprintf(ctx->expression, sizeof(ctx->expression),
                 "%.8g %c %s",
                 ctx->stored_value,
                 ctx->operation,
                 ctx->input);
    } else {
        snprintf(ctx->expression, sizeof(ctx->expression), "%s", ctx->input);
    }
}

/* Расставляет 17 кнопок относительно актуального размера окна. */
static void layout_buttons(struct Context *ctx)
{
    const char *labels[BUTTON_COUNT] = {
        "C", "/", "*",
        "7", "8", "9", "-",
        "4", "5", "6", "+",
        "1", "2", "3", "=",
        "0", "."
    };

    const char actions[BUTTON_COUNT] = {
        'C', '/', '*',
        '7', '8', '9', '-',
        '4', '5', '6', '+',
        '1', '2', '3', '=',
        '0', '.'
    };

    /*
       позиции кнопок: номер столбца и строки.
    */
    const int columns[BUTTON_COUNT] = {
        0, 1, 2,
        0, 1, 2, 3,
        0, 1, 2, 3,
        0, 1, 2, 3,
        0, 2
    };

    const int rows[BUTTON_COUNT] = {
        0, 0, 0,
        1, 1, 1, 1,
        2, 2, 2, 2,
        3, 3, 3, 3,
        4, 4
    };

    float margin = ctx->width * 0.06f;
    float gap = ctx->width * 0.025f;

    float panel_w = ctx->width - margin * 2.0f;
    float panel_h = ctx->height * 0.92f;
    float panel_x = margin;
    float panel_y = (ctx->height - panel_h) / 2.0f;

    float display_h = panel_h * 0.18f;
    float grid_x = panel_x + margin * 0.35f;
    float grid_y = panel_y + display_h + gap;
    float grid_w = panel_w - margin * 0.7f;
    float grid_h = panel_h - display_h - gap * 2.0f;

    float cell_w = (grid_w - gap * 3.0f) / 4.0f;
    float cell_h = (grid_h - gap * 4.0f) / 5.0f;

    for (int i = 0; i < BUTTON_COUNT; i++) {
        float button_w = cell_w;

        if (actions[i] == '0') {
            button_w = cell_w * 2.0f + gap;
        }

        ctx->buttons[i].rect = (SDL_FRect) {
            grid_x + columns[i] * (cell_w + gap),
            grid_y + rows[i] * (cell_h + gap),
            button_w,
            cell_h
        };

        ctx->buttons[i].label = labels[i];
        ctx->buttons[i].action = actions[i];
    }
}

static void press_button(struct Context *ctx, char action)
{
    if (action >= '0' && action <= '9') {
        if (ctx->waiting_for_number || strcmp(ctx->input, "0") == 0) {
            set_input(ctx, "");
            ctx->waiting_for_number = 0;
        }

        size_t len = strlen(ctx->input);
        if (len < sizeof(ctx->input) - 1) {
            ctx->input[len] = action;
            ctx->input[len + 1] = '\0';
        }

        ctx->result[0] = '\0';
        update_expression(ctx);
        return;
    }

    if (action == '.') {
        if (ctx->waiting_for_number) {
            set_input(ctx, "0");
            ctx->waiting_for_number = 0;
        }

        if (strchr(ctx->input, '.') == NULL) {
            size_t len = strlen(ctx->input);
            if (len < sizeof(ctx->input) - 1) {
                ctx->input[len] = '.';
                ctx->input[len + 1] = '\0';
            }
        }

        ctx->result[0] = '\0';
        update_expression(ctx);
        return;
    }

    if (action == 'C') {
        set_input(ctx, "0");
        ctx->expression[0] = '\0';
        ctx->result[0] = '\0';
        ctx->operation = '\0';
        ctx->stored_value = 0.0;
        ctx->waiting_for_number = 0;
        return;
    }

    if (action == '+' || action == '-' || action == '*' || action == '/') {
        ctx->stored_value = atof(ctx->input);
        ctx->operation = action;
        ctx->waiting_for_number = 1;
        ctx->result[0] = '\0';
        update_expression(ctx);
        return;
    }

    if (action == '=' && ctx->operation != '\0') {
        double current_value = atof(ctx->input);
        double answer = 0.0;

        switch (ctx->operation) {
            case '+':
                answer = ctx->stored_value + current_value;
                break;

            case '-':
                answer = ctx->stored_value - current_value;
                break;

            case '*':
                answer = ctx->stored_value * current_value;
                break;

            case '/':
                if (current_value == 0.0) {
                    set_input(ctx, "Error: divide by zero, u can`t");
                    ctx->operation = '\0';
                    ctx->waiting_for_number = 1;
                    update_expression(ctx);
                    return;
                }
                answer = ctx->stored_value / current_value;
                break;
        }

        snprintf(ctx->result, sizeof(ctx->result), "= %.10g", answer);
        snprintf(ctx->input, sizeof(ctx->input), "%.10g", answer);
        ctx->operation = '\0';
        ctx->waiting_for_number = 1;
        update_expression(ctx);
    }
}

SDL_AppResult SDL_AppInit(void **appstate, int argc, char *argv[])
{
    (void)argc;
    (void)argv;

    struct Context *ctx = SDL_calloc(1, sizeof(*ctx));
    if (ctx == NULL) {
        SDL_Log("Не удалось выделить память: %s", SDL_GetError());
        return SDL_APP_FAILURE;
    }

    *appstate = ctx;
    ctx->title = "Calculator";
    set_input(ctx, "0");

    if (!SDL_Init(SDL_INIT_VIDEO)) {
        SDL_Log("Не удалось инициализировать SDL: %s", SDL_GetError());
        return SDL_APP_FAILURE;
    }

    ctx->width = 420;
    ctx->height = 700;

    if (!SDL_CreateWindowAndRenderer(
            ctx->title,
            ctx->width,
            ctx->height,
            SDL_WINDOW_RESIZABLE,
            &ctx->window,
            &ctx->renderer)) {
        SDL_Log("Не удалось создать окно: %s", SDL_GetError());
        return SDL_APP_FAILURE;
    }

    SDL_SetRenderVSync(ctx->renderer, 1);

    return SDL_APP_CONTINUE;
}

SDL_AppResult SDL_AppIterate(void *appstate)
{
    struct Context *ctx = appstate;

    if (!SDL_GetRenderOutputSize(ctx->renderer, &ctx->width, &ctx->height)) {
        SDL_Log("Не удалось получить размер окна: %s", SDL_GetError());
        return SDL_APP_FAILURE;
    }

    layout_buttons(ctx);

    // Тёмный фон окна
    SDL_SetRenderDrawColor(ctx->renderer, 20, 23, 32, 255);
    SDL_RenderClear(ctx->renderer);

    // Панель калькулятора
    float margin = ctx->width * 0.06f;
    float panel_h = ctx->height * 0.92f;
    SDL_FRect panel = {
        margin,
        (ctx->height - panel_h) / 2.0f,
        ctx->width - margin * 2.0f,
        panel_h
    };

    SDL_SetRenderDrawColor(ctx->renderer, 34, 39, 52, 255);
    SDL_RenderFillRect(ctx->renderer, &panel);

    // поле дисплея
    SDL_FRect display = {
        panel.x + margin * 0.35f,
        panel.y + margin * 0.35f,
        panel.w - margin * 0.7f,
        panel.h * 0.16f
    };

    SDL_SetRenderDrawColor(ctx->renderer, 13, 16, 23, 255);
    SDL_RenderFillRect(ctx->renderer, &display);

    // cтрока введённого выражения
    SDL_SetRenderDrawColor(ctx->renderer, 180, 190, 210, 255);
    SDL_RenderDebugText(
        ctx->renderer,
        display.x + 12.0f,
        display.y + 12.0f,
        ctx->expression
    );

    /* ответ выводится отдельной строкой*/
    if (ctx->result[0] != '\0') {
        SDL_SetRenderDrawColor(ctx->renderer, 120, 220, 160, 255);
        SDL_RenderDebugText(
            ctx->renderer,
            display.x + 12.0f,
            display.y + 34.0f,
            ctx->result
        );
    }

    // Кнопки калькулятора
    for (int i = 0; i < BUTTON_COUNT; i++) {
        SDL_FRect button = ctx->buttons[i].rect;
        char action = ctx->buttons[i].action;

        // разные цвета для цифр и операций
        if (action == 'C') {
            SDL_SetRenderDrawColor(ctx->renderer, 200, 75, 80, 255);
        } else if (action == '+' || action == '-' ||
                   action == '*' || action == '/' || action == '=') {
            SDL_SetRenderDrawColor(ctx->renderer, 70, 115, 210, 255);
        } else {
            SDL_SetRenderDrawColor(ctx->renderer, 55, 63, 80, 255);
        }

        SDL_RenderFillRect(ctx->renderer, &button);

        // Лёгкая рамка кнопки
        SDL_SetRenderDrawColor(ctx->renderer, 95, 105, 125, 255);
        SDL_RenderRect(ctx->renderer, &button);

        // Центрируем короткую подпись в кнопке
        SDL_SetRenderDrawColor(ctx->renderer, 255, 255, 255, 255);

        float text_x = button.x + (button.w - 8.0f) / 2.0f;
        float text_y = button.y + (button.h - 8.0f) / 2.0f;

        SDL_RenderDebugText(
            ctx->renderer,
            text_x,
            text_y,
            ctx->buttons[i].label
        );
    }

    SDL_RenderPresent(ctx->renderer);
    return SDL_APP_CONTINUE;
}

SDL_AppResult SDL_AppEvent(void *appstate, SDL_Event *event)
{
    struct Context *ctx = appstate;

    if (event->type == SDL_EVENT_QUIT) {
        return SDL_APP_SUCCESS;
    }

    if (event->type == SDL_EVENT_MOUSE_BUTTON_DOWN &&
        event->button.button == SDL_BUTTON_LEFT) {

        float mouse_x = event->button.x;
        float mouse_y = event->button.y;

        for (int i = 0; i < BUTTON_COUNT; i++) {
            SDL_FRect r = ctx->buttons[i].rect;

            if (mouse_x >= r.x && mouse_x <= r.x + r.w &&
                mouse_y >= r.y && mouse_y <= r.y + r.h) {
                press_button(ctx, ctx->buttons[i].action);
                break;
            }
        }
    }

    return SDL_APP_CONTINUE;
}

void SDL_AppQuit(void *appstate, SDL_AppResult result)
{
    (void)result;

    struct Context *ctx = appstate;
    if (ctx != NULL) {
        SDL_DestroyRenderer(ctx->renderer);
        SDL_DestroyWindow(ctx->window);
        SDL_free(ctx);
    }

    SDL_Quit();
}
