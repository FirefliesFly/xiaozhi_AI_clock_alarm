import os
import subprocess
import glob
import time
import re

def sanitize_filename(filename):
    """
    将文件名转换为合法的C语言变量名
    """
    name_without_ext = os.path.splitext(filename)[0]
    sanitized = re.sub(r'[^a-zA-Z0-9_]', '_', name_without_ext)
    
    if sanitized and sanitized[0].isdigit():
        sanitized = '_' + sanitized
    
    if not sanitized:
        sanitized = 'gif_animation'
    
    sanitized = re.sub(r'_+', '_', sanitized)
    sanitized = sanitized.strip('_')
    
    return sanitized

def process_single_gif(gif_file, index, total):
    """
    处理单个GIF文件 - 简化稳定版本
    """
    print(f"\n🔄 正在处理 ({index}/{total}): {gif_file}")
    
    # 提取并清理文件名
    base_name = sanitize_filename(gif_file)
    original_base_name = os.path.splitext(gif_file)[0]
    
    if original_base_name != base_name:
        print(f"  🔧 文件名清理: '{original_base_name}' -> '{base_name}'")
    
    try:
        # 使用简单的subprocess.run，避免复杂的进程管理
        process = subprocess.Popen(
            ["gif2oled.exe"],
            stdin=subprocess.PIPE,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            text=True,  # 使用文本模式
            encoding='utf-8',
            errors='ignore'  # 忽略编码错误
        )
        
        # 构建完整的输入序列
        input_sequence = (
            f"./{gif_file}\n"  # 输入文件路径
            "100\n"            # 输入阈值
            f"{base_name}\n"   # 输入输出文件名
            "q\n"              # 退出程序
        )
        
        print("  ⏳ 开始处理...")
        
        # 执行并等待完成
        stdout, stderr = process.communicate(
            input=input_sequence,
            timeout=60  # 60秒超时
        )
        
        # 显示输出
        if stdout:
            print("  📋 程序输出:")
            for line in stdout.split('\n'):
                if line.strip():
                    print(f"    {line}")
        
        if stderr:
            print("  ⚠️  错误输出:")
            for line in stderr.split('\n'):
                if line.strip():
                    print(f"    {line}")
        
        # 检查是否生成对应的.h文件
        expected_h_file = f"{base_name}.h"
        if os.path.exists(expected_h_file):
            print(f"  ✅ 成功生成: {expected_h_file}")
            return True
        else:
            print(f"  ❌ 未找到生成的文件: {expected_h_file}")
            return False
            
    except subprocess.TimeoutExpired:
        print("  ⏰ 处理超时，强制终止...")
        process.kill()
        return False
    except Exception as e:
        print(f"  ❌ 处理异常: {e}")
        return False

def process_all_gifs():
    """
    自动处理所有GIF文件
    """
    gif_files = glob.glob("*.gif")
    
    if not gif_files:
        print("❌ 当前目录下没有找到GIF文件")
        return
    
    print(f"🎬 找到 {len(gif_files)} 个GIF文件需要处理:")
    
    # 显示文件名清理预览
    print("\n📝 文件名清理预览:")
    for gif in gif_files:
        original_name = os.path.splitext(gif)[0]
        sanitized_name = sanitize_filename(gif)
        if original_name != sanitized_name:
            print(f"  '{original_name}' -> '{sanitized_name}'")
        else:
            print(f"  '{original_name}' ✓ (合法)")
    
    # 处理所有GIF文件
    print("\n🔨 开始使用gif2oled.exe处理GIF文件...")
    
    successful_conversions = 0
    for i, gif_file in enumerate(gif_files):
        success = process_single_gif(gif_file, i+1, len(gif_files))
        if success:
            successful_conversions += 1
        
        # 在处理下一个文件前稍作休息
        if i < len(gif_files) - 1:
            print("\n⏳ 准备处理下一个文件...")
            time.sleep(1)
    
    print(f"\n✅ GIF文件处理完成: {successful_conversions}/{len(gif_files)} 个文件成功")

def process_converted_files():
    """
    处理所有转换后的.h文件
    """
    print("\n" + "="*50)
    print("🔄 开始使用convert_shell_proc.py处理.h文件...")
    time.sleep(1)
    
    # 获取所有非_anim的.h文件
    h_files = [f for f in glob.glob("*.h") if not f.endswith('_anim.h')]
    
    if not h_files:
        print("❌ 没有找到需要处理的.h文件")
        return
    
    print(f"找到 {len(h_files)} 个.h文件需要转换:")
    
    successful_conversions = 0
    for h_file in h_files:
        base_name = os.path.splitext(h_file)[0]
        output_file = f"{base_name}_anim.h"
        
        print(f"\n📄 正在转换: {h_file} -> {output_file}")
        
        try:
            cmd = [
                'python', 'convert_shell_proc.py',
                '-i', h_file,
                '-o', output_file,
                '-W', '128',
                '-H', '64'
            ]
            
            result = subprocess.run(cmd, capture_output=True, text=True, timeout=30)
            
            if result.returncode == 0:
                print(f"  ✅ 成功转换: {output_file}")
                successful_conversions += 1
            else:
                print(f"  ❌ 转换失败: {h_file}")
                if result.stderr.strip():
                    print(f"    错误: {result.stderr.strip()}")
                    
        except Exception as e:
            print(f"  ❌ 转换异常: {e}")
    
    print(f"\n🎉 转换完成: {successful_conversions}/{len(h_files)} 个文件")
    
    final_files = glob.glob("*_anim.h")
    if final_files:
        print(f"\n📋 生成的最终文件:")
        for file in final_files:
            print(f"  - {file}")

def main():
    """主函数"""
    if not os.path.exists("gif2oled.exe"):
        print("❌ 错误: 在当前目录下找不到 gif2oled.exe")
        exit(1)
    
    if not os.path.exists("convert_shell_proc.py"):
        print("❌ 错误: 在当前目录下找不到 convert_shell_proc.py")
        exit(1)
    
    print("🚀 开始自动处理GIF文件...")
    print("📍 工作目录:", os.getcwd())
    print("=" * 50)
    
    process_all_gifs()
    process_converted_files()
    
    print("=" * 50)
    print("\n✅ 所有处理完成！")

if __name__ == "__main__":
    main()