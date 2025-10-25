import re
import os
import sys
import argparse
from datetime import datetime

def parse_header_file(input_file):
    """
    解析.h文件，提取所有数组（支持有/无PROGMEM）
    返回: {数组名: 数组数据}
    """
    print(f"读取文件: {input_file}")
    
    try:
        with open(input_file, 'r', encoding='utf-8') as f:
            content = f.read()
    except FileNotFoundError:
        print(f"错误: 输入文件 '{input_file}' 不存在")
        return {}
    except Exception as e:
        print(f"错误: 读取文件失败 - {e}")
        return {}
    
    # 正则表达式匹配数组（支持有/无PROGMEM）
    patterns = [
        # 格式1: const uint8_t name[] PROGMEM = { ... }
        r'const\s+uint8_t\s+(\w+)\[\]\s*PROGMEM\s*=\s*\{([^}]+)\}',
        # 格式2: const uint8_t name[] = { ... }
        r'const\s+uint8_t\s+(\w+)\[\]\s*=\s*\{([^}]+)\}',
        # 格式3: static const unsigned char name[] = { ... }
        r'static\s+const\s+unsigned\s+char\s+(\w+)\[\]\s*=\s*\{([^}]+)\}',
        # 格式4: const unsigned char name[] = { ... }
        r'const\s+unsigned\s+char\s+(\w+)\[\]\s*=\s*\{([^}]+)\}'
    ]
    
    arrays = {}
    for pattern in patterns:
        matches = re.findall(pattern, content, re.DOTALL)
        for array_name, array_content in matches:
            # 避免重复（如果多个模式匹配到同一个数组）
            if array_name in arrays:
                continue
                
            # 清理数组内容
            cleaned = re.sub(r'//.*?$', '', array_content, flags=re.MULTILINE)  # 移除注释
            cleaned = re.sub(r'/\*.*?\*/', '', cleaned, flags=re.DOTALL)  # 移除块注释
            cleaned = cleaned.replace('\n', ' ').replace('\t', ' ')
            cleaned = re.sub(r'\s+', ' ', cleaned).strip()
            
            # 解析十六进制值
            values = []
            parts = cleaned.split(',')
            for part in parts:
                part = part.strip()
                if part:
                    if part.startswith('0x'):
                        try:
                            values.append(int(part, 16))
                        except ValueError:
                            print(f"警告: 无法解析值 '{part}' 在数组 {array_name}")
                    else:
                        try:
                            values.append(int(part))
                        except ValueError:
                            print(f"警告: 无法解析值 '{part}' 在数组 {array_name}")
            
            arrays[array_name] = values
            # 修复：使用ASCII字符替代Unicode字符
            print(f"[OK] 提取数组: {array_name} ({len(values)} 字节)")
    
    return arrays

def convert_to_column_major(data_row, screen_width=128, screen_height=64):
    """
    行优先转列优先（低位在前）
    """
    bytes_per_row = screen_width // 8
    bytes_per_column = screen_height // 8
    data_column = [0] * len(data_row)
    
    for col in range(screen_width):
        for page in range(bytes_per_column):
            new_byte = 0
            for row_in_page in range(8):
                row = page * 8 + row_in_page
                byte_in_row = col // 8
                bit_in_byte = 7 - (col % 8)
                
                original_byte = data_row[row * bytes_per_row + byte_in_row]
                pixel_bit = (original_byte >> bit_in_byte) & 1
                new_byte |= (pixel_bit << row_in_page)  # 低位在前
            
            target_index = col + page * screen_width
            data_column[target_index] = new_byte
    
    return data_column

def validate_conversion(data_row, data_column, screen_width=128, screen_height=64):
    """
    验证转换正确性
    """
    bytes_per_row = screen_width // 8
    errors = []
    
    for y in range(screen_height):
        for x in range(screen_width):
            # 原始像素（行优先）
            byte_idx_row = y * bytes_per_row + (x // 8)
            bit_idx_row = 7 - (x % 8)
            original = (data_row[byte_idx_row] >> bit_idx_row) & 1
            
            # 转换后像素（列优先）
            page = y // 8
            row_in_page = y % 8
            byte_idx_col = x + page * screen_width
            bit_idx_col = row_in_page  # 低位在前
            converted = (data_column[byte_idx_col] >> bit_idx_col) & 1
            
            if original != converted:
                errors.append((x, y, original, converted))
    
    return errors

def generate_output_header(input_arrays, output_file, input_filename):
    """
    生成输出.h文件
    """
    print(f"\n生成输出文件: {output_file}")
    
    try:
        with open(output_file, 'w', encoding='utf-8') as f:
            # 写入文件头
            f.write("// 自动生成的列优先格式图像数据\n")
            f.write("// 由行优先格式转换而来\n")
            f.write(f"// 源文件: {input_filename}\n")
            f.write(f"// 生成时间: {datetime.now().strftime('%Y-%m-%d %H:%M:%S')}\n")
            f.write("#ifndef CONVERTED_IMAGES_H\n")
            f.write("#define CONVERTED_IMAGES_H\n\n")
            f.write("#include <avr/pgmspace.h>\n\n")
            
            # 写入每个转换后的数组
            for array_name, data_column in input_arrays.items():
                f.write(f"// 转换自: {array_name} (列优先格式)\n")
                f.write(f"const uint8_t {array_name}_column[] PROGMEM = {{\n")
                
                # 每行16个字节
                for i in range(0, len(data_column), 16):
                    line = data_column[i:i+16]
                    hex_line = [f"0x{byte:02X}" for byte in line]
                    f.write("  " + ", ".join(hex_line) + ",\n")
                
                f.write("};\n\n")
            
            f.write("#endif // CONVERTED_IMAGES_H\n")
        
        print("[OK] 输出文件生成完成")
        return True
    except Exception as e:
        print(f"错误: 生成输出文件失败 - {e}")
        return False

def main():
    # 创建命令行参数解析器
    parser = argparse.ArgumentParser(description='批量转换图像数组工具')
    parser.add_argument('-i', '--input', required=True, help='输入.h文件路径')
    parser.add_argument('-o', '--output', required=True, help='输出.h文件路径')
    parser.add_argument('-W', '--width', type=int, default=128, help='图像宽度 (默认: 128)')
    parser.add_argument('-H', '--height', type=int, default=64, help='图像高度 (默认: 64)')
    
    # 解析参数
    args = parser.parse_args()
    
    # 配置参数
    input_file = args.input
    output_file = args.output
    screen_width = args.width
    screen_height = args.height
    expected_bytes = screen_width * screen_height // 8
    
    print("=" * 60)
    print("批量转换图像数组工具")
    print("=" * 60)
    print(f"输入文件: {input_file}")
    print(f"输出文件: {output_file}")
    print(f"图像尺寸: {screen_width} × {screen_height}")
    print(f"预期字节数: {expected_bytes}")
    print("=" * 60)
    
    # 1. 解析输入文件
    arrays = parse_header_file(input_file)
    
    if not arrays:
        print("错误: 未找到任何数组")
        return 1
    
    print(f"\n找到 {len(arrays)} 个数组:")
    for name in arrays.keys():
        print(f"  - {name}")
    
    # 2. 转换每个数组
    converted_arrays = {}
    conversion_results = {}
    
    print(f"\n开始转换数组...")
    for array_name, data in arrays.items():
        print(f"\n处理数组: {array_name}")
        
        # 验证数据长度
        if len(data) != expected_bytes:
            print(f"  错误: 数据长度错误: {len(data)} 字节 (预期: {expected_bytes})")
            continue
        
        # 执行转换
        converted_data = convert_to_column_major(data, screen_width, screen_height)
        
        # 验证转换
        errors = validate_conversion(data, converted_data, screen_width, screen_height)
        
        if errors:
            print(f"  警告: 转换完成，发现 {len(errors)} 个错误")
            error_rows = set([y for x, y, _, _ in errors])
            print(f"     涉及行: {sorted(error_rows)[:5]}...")  # 只显示前5行
        else:
            print(f"  [OK] 转换完成，验证通过")
        
        converted_arrays[array_name] = converted_data
        conversion_results[array_name] = {
            'original_length': len(data),
            'converted_length': len(converted_data),
            'errors': len(errors)
        }
    
    # 3. 生成输出文件
    if converted_arrays:
        success = generate_output_header(converted_arrays, output_file, input_file)
        
        if success:
            # 4. 显示统计信息
            print(f"\n{'='*60}")
            print("转换统计:")
            print(f"{'='*60}")
            successful = sum(1 for result in conversion_results.values() if result['errors'] == 0)
            total = len(conversion_results)
            
            print(f"成功转换: {successful}/{total} 个数组")
            
            for array_name, result in conversion_results.items():
                status = "[OK]" if result['errors'] == 0 else "[WARN]"
                print(f"  {status} {array_name}: {result['original_length']}字节 -> {result['converted_length']}字节, 错误: {result['errors']}个")
            
            print(f"\n输出文件已保存至: {output_file}")
            return 0
        else:
            return 1
    else:
        print("错误: 没有成功转换任何数组")
        return 1

if __name__ == "__main__":
    sys.exit(main())