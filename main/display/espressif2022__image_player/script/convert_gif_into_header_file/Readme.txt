（1）convert_shell_proc.py用来进行行转列（也即黑白色OLED是列行式采写屏幕，且是逆向，也即对于一个字节来说，低位的像素在前面显示，即低位在前），并覆盖生成对应的.h文件，以便适配OLED的驱动规律
（2）process_gif_shell.py负责调用gif2oled.exe程序对gif文件进行批量处理（如果GIF文件名不符合C语言变量的命名规则，则将非法字符用字符下划线'_'进行替代），处理完成后继续调用convert_shell_proc.py文件进行批量处理，从而得到对应的、OLED能够使用的数组图像库。
使用方法：python 你的脚本文件名.py
说明：该脚本会自动遍历相对路径下的所有gif文件。
（3）gif2oled.exe负责将GIF转成OLED可用的图像（逐行式擦写屏幕，且高位在前），所以对于黑白色的OLED而言，得转成（列行式，逆向【即低位在前】）的形式才行，所以需要用到convert_shell_proc.py脚本