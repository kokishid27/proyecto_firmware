idf_component_register(SRCS "hal_board.c"
                    INCLUDE_DIRS "include"
                    REQUIRES driver_gpio
                    REQUIRES driver_timer)
