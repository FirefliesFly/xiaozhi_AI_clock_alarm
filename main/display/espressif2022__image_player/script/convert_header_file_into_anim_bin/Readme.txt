（1）使用说明：处理target_h_file文件目录下的所有.h文件（每一个动画文件为一个.h文件），针对黑白的OLED而言，位深应该为1，splits可设置为1，-W为屏幕宽度，-H为屏幕高度。.h文件里的数组的长度应当等于W*H，否则无用。
python convert_header_file_into_bin_for_anim_v5.py target_h_file -W 128 -H 64 -b 1 -s 1 -v "V1.00"

