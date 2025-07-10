# `idf_component_register`注册一个esp-idf项目的组件
# 左括号"("不能换行
file(GLOB DRIVERS_C "dri/*.c")
idf_component_register(
    # .c文件的相对路径
    SRCS "main.c" ${DRIVERS_C}
    # "dri/Audio.c" "dri/Fingerprint.c" "dri/keyboard.c" "dri/Motor.c" "dri/LED.c"
    # 头文件所在的!!文件夹!!路径
    INCLUDE_DIRS "dri/"
)
