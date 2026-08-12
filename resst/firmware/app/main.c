#include "firmware/app/app.h"

static AppContext g_app;

void main(void)
{
    App_Init(&g_app);
    App_RunForever(&g_app);
}
