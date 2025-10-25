import os
import re
import struct
import hashlib
from pathlib import Path
from typing import List, Dict, Tuple, Optional

class RLEEncoder:
    """RLE编码器 - 针对1位深度优化"""
    
    # 编码类型常量
    ENCODING_TYPE_RLE = 0x01
    
    @staticmethod
    def encode(data: bytes, bit_depth: int = 1) -> bytes:
        """
        RLE编码实现 - 针对不同位深度优化
        
        Args:
            data: 输入数据
            bit_depth: 位深度 (1, 4, 8)
            
        Returns:
            bytes: 压缩后的数据（包含编码类型标识符）
        """
        if not data:
            return bytes([RLEEncoder.ENCODING_TYPE_RLE])
        
        compressed = bytearray()
        
        # 添加编码类型标识符
        compressed.append(RLEEncoder.ENCODING_TYPE_RLE)
        
        # 对于1位深度，直接对字节数据进行RLE编码
        # 因为1位数据已经是打包格式（每字节8像素）
        i = 0
        n = len(data)
        
        while i < n:
            count = 1
            current_byte = data[i]
            
            # 计算连续相同字节的数量，最大255
            while (i + count < n and 
                   data[i + count] == current_byte and 
                   count < 255):
                count += 1
            
            # 写入[count, value]对
            compressed.append(count)
            compressed.append(current_byte)
            
            i += count
        
        return bytes(compressed)
    
    @staticmethod
    def decode(compressed_data: bytes, expected_output_len: int = None, bit_depth: int = 1) -> bytes:
        """
        RLE解码实现
        """
        if not compressed_data:
            raise ValueError("无效的RLE数据：数据为空")
        
        # 检查编码类型
        encoding_type = compressed_data[0]
        if encoding_type != RLEEncoder.ENCODING_TYPE_RLE:
            raise ValueError(f"不支持的编码类型: 0x{encoding_type:02X}")
        
        # 跳过编码类型标识符
        rle_data = compressed_data[1:]
        
        decoded = bytearray()
        in_pos = 0
        input_len = len(rle_data)
        
        while in_pos + 1 < input_len:
            count = rle_data[in_pos]
            value = rle_data[in_pos + 1]
            in_pos += 2
            
            # 检查输出缓冲区溢出
            if expected_output_len and len(decoded) + count > expected_output_len:
                raise ValueError(f"输出缓冲区溢出: {len(decoded) + count} > {expected_output_len}")
            
            # 重复count次value
            decoded.extend([value] * count)
        
        return bytes(decoded)
    
    @staticmethod
    def debug_rle_data(compressed_data: bytes, max_display: int = 20):
        """调试RLE数据"""
        if not compressed_data:
            print("RLE数据为空")
            return
        
        encoding_type = compressed_data[0]
        print(f"编码类型: 0x{encoding_type:02X}")
        
        rle_data = compressed_data[1:]
        print(f"RLE数据长度: {len(rle_data)} 字节")
        print(f"RLE数据: {rle_data.hex()}")
        
        # 显示前几个RLE对
        in_pos = 0
        pair_count = 0
        while in_pos + 1 < len(rle_data) and pair_count < max_display:
            count = rle_data[in_pos]
            value = rle_data[in_pos + 1]
            print(f"  对{pair_count}: count={count}, value=0x{value:02X} (二进制: {value:08b})")
            in_pos += 2
            pair_count += 1
        
        if in_pos < len(rle_data):
            print(f"  ... 还有 {len(rle_data) - in_pos} 字节未显示")
    
    @staticmethod
    def debug_original_data(data: bytes, max_display: int = 50, bit_depth: int = 1):
        """调试原始数据"""
        print(f"原始数据长度: {len(data)} 字节")
        
        if bit_depth == 1:
            print("1位深度数据解析 (每字节8像素):")
            for i in range(min(max_display, len(data))):
                byte_val = data[i]
                binary_str = f"{byte_val:08b}"
                pixel_str = ' '.join(['█' if bit == '1' else ' ' for bit in binary_str])
                print(f"  字节{i:03d}: 0x{byte_val:02X} | {binary_str} | {pixel_str}")
        else:
            print(f"前{min(max_display, len(data))}字节:")
            for i in range(min(max_display, len(data))):
                if i % 16 == 0:
                    print(f"  {i:04d}: ", end="")
                print(f"{data[i]:02X} ", end="")
                if i % 16 == 15:
                    print()
            if min(max_display, len(data)) % 16 != 0:
                print()

class AnimationHeader:
    """动画文件头部协议处理类"""
    
    # 常量定义
    ASSETS_FILE_MAGIC_HEAD = 0x5A5A
    ASSETS_FILE_MAGIC_LEN = 2
    
    def __init__(self, frame_count: int = 0):
        self.frame_count = frame_count
        self.checksum = 0
        self.table_length = frame_count * 8
        self.asset_table = []
    
    def add_frame_info(self, frame_size: int, frame_offset: int):
        self.asset_table.append((frame_size, frame_offset))
    
    def calculate_checksum(self, frame_data: bytes) -> int:
        checksum = 0
        for byte in frame_data:
            checksum = (checksum + byte) & 0xFFFFFFFF
        self.checksum = checksum
        return checksum
    
    def to_bytes(self) -> bytes:
        header_data = bytearray()
        header_data.extend(struct.pack('<I', self.frame_count))
        header_data.extend(struct.pack('<I', self.checksum))
        header_data.extend(struct.pack('<I', self.table_length))
        for asset_size, asset_offset in self.asset_table:
            header_data.extend(struct.pack('<I', asset_size))
            header_data.extend(struct.pack('<I', asset_offset))
        return bytes(header_data)
    
    def get_total_header_size(self) -> int:
        return 12 + self.table_length

class FrameHeader:
    """单帧头部协议处理类"""
    
    def __init__(self, width: int, height: int, splits: int, bit_depth: int = 1, version: str = "V1.00"):
        self.format = b'_S'
        self.version = version.encode('ascii').ljust(6, b'\x00')
        self.bit_depth = bit_depth
        self.width = width
        self.height = height
        self.splits = splits
        self.split_height = height // splits if splits > 0 else height
        self.split_lengths = []
        self.palette_data = b''
    
    def set_split_lengths(self, split_lengths: List[int]):
        self.split_lengths = split_lengths
    
    def set_palette_data(self, palette_data: bytes):
        self.palette_data = palette_data
    
    def to_bytes(self) -> bytes:
        header_data = bytearray()
        header_data.extend(self.format.ljust(3, b'\x00'))
        header_data.extend(self.version)
        header_data.extend(struct.pack('<B', self.bit_depth))
        header_data.extend(struct.pack('<H', self.width))
        header_data.extend(struct.pack('<H', self.height))
        header_data.extend(struct.pack('<H', self.splits))
        header_data.extend(struct.pack('<H', self.split_height))
        for length in self.split_lengths:
            header_data.extend(struct.pack('<H', length))
        
        # 对于1位深度，调色板只需要2种颜色
        palette_size = (1 << self.bit_depth) * 4
        if len(self.palette_data) < palette_size:
            self.palette_data = self._generate_default_palette()
        header_data.extend(self.palette_data[:palette_size])
        
        return bytes(header_data)
    
    def _generate_default_palette(self) -> bytes:
        palette_data = bytearray()
        num_colors = 1 << self.bit_depth
        
        if self.bit_depth == 1:
            # 1位：黑白调色板 (黑, 白)
            colors = [(0, 0, 0), (255, 255, 255)]
        elif self.bit_depth == 4:
            colors = [(i * 17, i * 17, i * 17) for i in range(16)]
        else:
            colors = [(i, i, i) for i in range(256)]
        
        for color in colors:
            palette_data.extend([color[2], color[1], color[0], 0])
        
        while len(palette_data) < (num_colors * 4):
            palette_data.extend([0, 0, 0, 0])
        
        return bytes(palette_data)

class HFileParser:
    """.h文件解析器"""
    
    def __init__(self):
        # 增强正则表达式，匹配更多格式，包括PROGMEM关键字
        self.array_pattern = re.compile(
            r'const\s+uint8_t\s+(\w+)\s*\[\s*\]\s*(?:PROGMEM\s*)?=\s*\{([^}]+)\}\s*;',
            re.MULTILINE | re.DOTALL
        )
        self.hex_pattern = re.compile(r'0x([0-9A-Fa-f]{1,2})')
    
    def parse_h_file(self, file_path: str) -> Tuple[Dict[str, bytes], List[str]]:
        """
        解析.h文件，提取所有数组数据，保持出现顺序
        
        Returns:
            Tuple[字典, 顺序列表]: (数组字典, 数组出现顺序列表)
        """
        try:
            with open(file_path, 'r', encoding='utf-8') as f:
                content = f.read()
            
            arrays = {}
            array_order = []  # 保存数组出现顺序
            
            # 使用 finditer 来保持顺序
            matches = list(self.array_pattern.finditer(content))
            
            print(f"  在文件中找到 {len(matches)} 个数组")
            
            for match in matches:
                array_name = match.group(1)
                array_data = match.group(2)
                
                # 记录数组出现顺序
                array_order.append(array_name)
                
                # 提取十六进制数据
                hex_values = self.hex_pattern.findall(array_data)
                
                if not hex_values:
                    print(f"    ⚠️  警告: 数组 {array_name} 没有找到有效的十六进制数据")
                    continue
                
                # 转换为字节数据
                try:
                    byte_data = bytes(int(hex_val, 16) for hex_val in hex_values)
                    arrays[array_name] = byte_data
                    print(f"    ✅ 数组: {array_name}, 数据长度: {len(byte_data)} 字节")
                    
                except ValueError as e:
                    print(f"    ❌ 错误: 数组 {array_name} 数据转换失败: {e}")
                    continue
            
            print(f"  数组出现顺序: {array_order}")
            return arrays, array_order
            
        except Exception as e:
            print(f"  解析文件错误: {e}")
            return {}, []
    
    def validate_frame_data(self, frame_data: bytes, width: int, height: int, bit_depth: int) -> bool:
        expected_size = self.calculate_expected_size(width, height, bit_depth)
        actual_size = len(frame_data)
        
        if actual_size != expected_size:
            print(f"    警告: 数据长度不匹配! 期望: {expected_size} 字节, 实际: {actual_size} 字节")
            return False
        else:
            print(f"    数据尺寸验证通过: {actual_size} 字节")
            return True

    def calculate_expected_size(self, width: int, height: int, bit_depth: int) -> int:
        if bit_depth == 1:
            # 1位：每个字节包含8个像素
            return (width * height + 7) // 8
        elif bit_depth == 4:
            return (width * height + 1) // 2
        else:
            return width * height

class AnimationProcessor:
    """动画处理器"""
    
    def __init__(self, output_dir: str = "output", 
                 width: int = 128, height: int = 64, 
                 splits: int = 8, bit_depth: int = 1,
                 version: str = "V1.00", enable_rle: bool = True):
        self.output_dir = Path(output_dir)
        self.output_dir.mkdir(exist_ok=True, parents=True)
        self.width = width
        self.height = height
        self.splits = splits
        self.bit_depth = bit_depth
        self.version = version
        self.enable_rle = enable_rle
        self.h_parser = HFileParser()
        
        print(f"动画参数: {width}x{height}, 位深度: {bit_depth}, 分块数: {splits}, 版本: {version}")
        print(f"RLE编码: {'启用' if enable_rle else '禁用'}")

    def create_frame_with_header(self, frame_data: bytes, frame_index: int, array_name: str) -> Tuple[bytes, int, int]:
        """为帧数据创建完整的帧数据（包含协议头）"""
        # 验证数据尺寸
        expected_size = self.h_parser.calculate_expected_size(self.width, self.height, self.bit_depth)
        actual_size = len(frame_data)
        
        print(f"    📏 数据尺寸检查: 期望 {expected_size} 字节, 实际 {actual_size} 字节")
        
        if actual_size != expected_size:
            print(f"    ❌ 错误: 数据尺寸不匹配! 数组 {array_name} 将被跳过")
            return b'', 0, 0
        
        # 创建帧头部
        frame_header = FrameHeader(self.width, self.height, self.splits, self.bit_depth, self.version)
        
        # 对图像数据进行RLE编码（如果启用）
        image_data = frame_data
        original_size = len(image_data)
        compressed_size = original_size
        
        if self.enable_rle:
            try:
                print(f"    🔍 调试数组 {array_name}:")
                RLEEncoder.debug_original_data(frame_data, 5, self.bit_depth)
                
                image_data = RLEEncoder.encode(frame_data, self.bit_depth)
                compressed_size = len(image_data)
                
                RLEEncoder.debug_rle_data(image_data, 5)
                
                # 验证编码
                decoded_data = RLEEncoder.decode(image_data, original_size, self.bit_depth)
                
                if decoded_data == frame_data:
                    compression_ratio = (1 - (compressed_size / original_size)) * 100
                    print(f"    ✅ RLE压缩成功: {original_size} → {compressed_size} 字节 (压缩率: {compression_ratio:+.1f}%)")
                else:
                    print(f"    ⚠️  RLE验证失败! 使用未压缩数据")
                    # 找出不匹配的位置
                    mismatch_count = 0
                    for i in range(min(len(decoded_data), len(frame_data))):
                        if decoded_data[i] != frame_data[i]:
                            mismatch_count += 1
                            if mismatch_count <= 3:  # 只显示前3个不匹配
                                print(f"      不匹配位置 {i}: 解码=0x{decoded_data[i]:02X}, 原始=0x{frame_data[i]:02X}")
                    print(f"      总共 {mismatch_count} 个不匹配位置")
                    
                    image_data = frame_data
                    compressed_size = original_size
                    
            except Exception as e:
                print(f"    ❌ RLE编码错误: {e}，使用未压缩数据")
                image_data = frame_data
                compressed_size = original_size
        else:
            print(f"    📦 未压缩: {original_size} 字节")
        
        # 分块长度计算
        split_lengths = []
        if self.splits > 1:
            split_size = compressed_size // self.splits
            for i in range(self.splits):
                if i == self.splits - 1:
                    split_lengths.append(compressed_size - (split_size * (self.splits - 1)))
                else:
                    split_lengths.append(split_size)
        else:
            split_lengths = [compressed_size]
        
        frame_header.set_split_lengths(split_lengths)
        
        # 构建完整的帧数据
        frame_with_header = bytearray()
        frame_with_header.extend(struct.pack('<H', AnimationHeader.ASSETS_FILE_MAGIC_HEAD))
        frame_with_header.extend(frame_header.to_bytes())
        frame_with_header.extend(image_data)
        
        header_size = len(frame_header.to_bytes())
        total_size = len(frame_with_header)
        
        print(f"    ✅ 帧处理完成: 总大小 {total_size} 字节")
        
        return bytes(frame_with_header), original_size, compressed_size
    
    def process_single_h_file(self, h_file_path: str) -> Optional[str]:
        """处理单个.h文件"""
        try:
            h_file_name = Path(h_file_path).stem
            print(f"\n🎬 开始处理文件: {h_file_path}")
            
            # 解析.h文件，获取数组和顺序信息
            arrays, array_order = self.h_parser.parse_h_file(h_file_path)
            if not arrays:
                print(f"  ❌ 错误: 在 {h_file_path} 中未找到数组数据")
                return None
            
            frame_count = len(arrays)
            print(f"  📊 总数组数: {frame_count}")
            
            animation_header = AnimationHeader(frame_count)
            all_frame_data = bytearray()
            current_offset = animation_header.get_total_header_size()
            
            total_original_size = 0
            total_compressed_size = 0
            processed_frames = 0
            skipped_frames = 0
            
            # 按照数组在文件中的出现顺序处理
            print(f"  🔄 按照出现顺序处理数组")
            
            for frame_index, array_name in enumerate(array_order):
                if array_name not in arrays:
                    print(f"  ⚠️  警告: 数组 {array_name} 在字典中不存在，跳过")
                    skipped_frames += 1
                    continue
                    
                frame_data = arrays[array_name]
                print(f"\n  🖼️  处理帧 {frame_index}: {array_name}")
                
                frame_with_header, original_size, compressed_size = self.create_frame_with_header(
                    frame_data, frame_index, array_name
                )
                
                if frame_with_header:
                    total_original_size += original_size
                    total_compressed_size += compressed_size
                    
                    frame_size = len(frame_with_header)
                    all_frame_data.extend(frame_with_header)
                    animation_header.add_frame_info(frame_size, current_offset)
                    current_offset += frame_size
                    processed_frames += 1
                    print(f"  ✅ 成功处理帧 {frame_index}")
                else:
                    skipped_frames += 1
                    print(f"  ❌ 跳过帧 {frame_index}")
            
            print(f"\n📈 处理统计:")
            print(f"  成功处理: {processed_frames}/{frame_count} 帧")
            print(f"  跳过: {skipped_frames}/{frame_count} 帧")
            
            if processed_frames == 0:
                print(f"  ❌ 错误: 没有成功处理任何帧!")
                return None
            
            # 计算校验和
            animation_header.calculate_checksum(all_frame_data)
            
            # 构建完整的.bin文件
            bin_data = bytearray()
            bin_data.extend(animation_header.to_bytes())
            bin_data.extend(all_frame_data)
            
            # 保存文件
            bin_file_path = self.output_dir / f"{h_file_name}.bin"
            self.output_dir.mkdir(exist_ok=True, parents=True)
            
            absolute_bin_path = bin_file_path.resolve()
            print(f"  💾 保存文件到: {absolute_bin_path}")
            
            try:
                with open(absolute_bin_path, 'wb') as f:
                    f.write(bin_data)
            except OSError as e:
                print(f"  ❌ 文件保存错误: {e}")
                # 尝试使用不同的文件名
                safe_bin_path = self.output_dir / f"{h_file_name}_animation.bin"
                absolute_safe_path = safe_bin_path.resolve()
                print(f"  🔄 尝试使用安全路径: {absolute_safe_path}")
                with open(absolute_safe_path, 'wb') as f:
                    f.write(bin_data)
                bin_file_path = safe_bin_path
            
            total_header_size = animation_header.get_total_header_size()
            total_frame_size = len(all_frame_data)
            total_file_size = len(bin_data)
            
            print(f"\n✅ 生成文件: {bin_file_path}")
            print(f"📊 统计信息:")
            print(f"  总帧数: {processed_frames}")
            print(f"  动画头部大小: {total_header_size} 字节")
            print(f"  帧数据总大小: {total_frame_size} 字节") 
            print(f"  文件总大小: {total_file_size} 字节")
            print(f"  资源表大小: {animation_header.table_length} 字节")
            print(f"  校验和: 0x{animation_header.checksum:08X}")
            
            if self.enable_rle:
                overall_compression_ratio = (1 - total_compressed_size / total_original_size) * 100
                print(f"  📦 压缩统计:")
                print(f"    原始数据: {total_original_size} 字节")
                print(f"    压缩后: {total_compressed_size} 字节")
                print(f"    总体压缩率: {overall_compression_ratio:+.1f}%")
            
            return str(bin_file_path)
            
        except Exception as e:
            print(f"  ❌ 错误处理文件 {h_file_path}: {e}")
            import traceback
            traceback.print_exc()
            return None
    
    def process_directory(self, input_dir: str):
        """处理目录中的所有.h文件"""
        input_path = Path(input_dir)
        
        if not input_path.exists():
            print(f"❌ 错误: 目录 {input_dir} 不存在")
            return
        
        h_files = list(input_path.glob("*.h"))
        if not h_files:
            print(f"📁 在 {input_dir} 中未找到.h文件")
            return
        
        print(f"🔍 找到 {len(h_files)} 个.h文件，开始处理...")
        
        success_count = 0
        for h_file in h_files:
            result = self.process_single_h_file(str(h_file))
            if result:
                success_count += 1
        
        print(f"\n🎉 处理完成: 成功 {success_count}/{len(h_files)} 个文件")
    
    def batch_process(self, input_dirs: List[str]):
        """批量处理多个目录"""
        for input_dir in input_dirs:
            print(f"\n{'='*60}")
            print(f"📂 处理目录: {input_dir}")
            print(f"{'='*60}")
            self.process_directory(input_dir)

def test_1bit_rle():
    """测试1位深度的RLE编码"""
    print("=== 1位深度RLE测试 ===")
    
    # 创建测试数据：简单的1位图案
    # 每字节8像素：0=黑, 1=白
    test_data = bytes([
        0b11111111,  # 8个白像素
        0b11111111,  # 8个白像素  
        0b00000000,  # 8个黑像素
        0b00000000,  # 8个黑像素
        0b10101010,  # 交替黑白
        0b10101010,  # 交替黑白
    ])
    
    print("测试数据:")
    RLEEncoder.debug_original_data(test_data, len(test_data), 1)
    
    encoded = RLEEncoder.encode(test_data, 1)
    print("\n编码后:")
    RLEEncoder.debug_rle_data(encoded, 10)
    
    decoded = RLEEncoder.decode(encoded, len(test_data), 1)
    print("\n解码后:")
    RLEEncoder.debug_original_data(decoded, len(decoded), 1)
    
    print(f"\n匹配: {decoded == test_data}")

def main():
    """主函数"""
    import argparse
    
    parser = argparse.ArgumentParser(description='将.h文件中的动画数据转换为.bin文件')
    parser.add_argument('input', nargs='+', help='输入目录或.h文件路径')
    parser.add_argument('-o', '--output', default='output', help='输出目录')
    parser.add_argument('-W', '--width', type=int, required=True, help='图像宽度（像素）')
    parser.add_argument('-H', '--height', type=int, required=True, help='图像高度（像素）')
    parser.add_argument('-b', '--bit-depth', type=int, choices=[1, 4, 8], default=1, 
                       help='位深度 (1, 4, 8)，默认: 1')
    parser.add_argument('-s', '--splits', type=int, default=8, 
                       help='分块数，默认: 8')
    parser.add_argument('-v', '--version', default='V1.00', 
                       help='版本字符串，默认: V1.00')
    parser.add_argument('--no-rle', action='store_true', 
                       help='禁用RLE编码')
    
    args = parser.parse_args()
    
    # 验证参数
    if args.width <= 0 or args.height <= 0:
        print("❌ 错误: 宽度和高度必须是正整数")
        return
    
    if args.splits <= 0:
        print("❌ 错误: 分块数必须是正整数")
        return
    
    if args.height % args.splits != 0:
        print(f"⚠️  警告: 高度 {args.height} 不能被分块数 {args.splits} 整除")
    
    processor = AnimationProcessor(
        output_dir=args.output,
        width=args.width,
        height=args.height,
        splits=args.splits,
        bit_depth=args.bit_depth,
        version=args.version,
        enable_rle=not args.no_rle
    )
    
    # 处理所有输入路径
    for input_path in args.input:
        if os.path.isfile(input_path) and input_path.endswith('.h'):
            # 处理单个文件
            processor.process_single_h_file(input_path)
        elif os.path.isdir(input_path):
            # 处理目录
            processor.process_directory(input_path)
        else:
            print(f"⚠️  警告: 跳过无效路径 {input_path}")

if __name__ == "__main__":
    # 可以选择是否运行测试
    run_test = input("是否运行RLE测试？(y/n, 默认n): ").lower().strip()
    if run_test == 'y':
        test_1bit_rle()
        print("\n" + "="*60 + "\n")
    
    # 运行主程序
    main()