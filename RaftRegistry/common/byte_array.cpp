#include "byte_array.h"
    
#include <spdlog/spdlog.h> // Include the header file that defines the SPDLOG_LOGGER_ERROR macro
#include <fstream>
#include <sstream>
#include <iomanip>


namespace RR {

static auto logger = GetLoggerInstance();

// 构造函数
ByteArray::ByteArray(size_t size) : m_nodeSize(size), m_position(0), m_capacity(size), m_size(0), m_endian(std::endian::big), m_head(new Node(size)), m_current(m_head) {};

ByteArray::~ByteArray() {
    Node* tmp = m_head,*current = m_head;
    while(tmp) {
        current = tmp;
        tmp = tmp->next;
        delete current;
    }
}

ByteArray::Node::Node() : ptr(nullptr), next(nullptr), size(0) {}
ByteArray::Node::Node(size_t size) : ptr(new char[size]()), next(nullptr), size(size) {}

ByteArray::Node::~Node() {
    if (ptr) {
        delete[] ptr;
    }
    ptr = nullptr;
}
// 构造函数结束

// 写入数据

void ByteArray::write(const void* buf, size_t size) {
    if (size == 0) {
        return;
    }
    addCapacity(size);

    size_t nodePos = m_position%m_nodeSize;
    size_t bufPos = 0;
    size_t nodeCap = m_nodeSize - nodePos;

    // 将buf中的数据写入链表中
    while(size > 0) {
        // 如果当前node的剩余容量可以容纳buf中所有字符，则直接将buf中的数据写入当前node中;
        // 否则，先填满当前node，然后将m_current指向下一个node，再次检测
        if (nodeCap >= size) {
            memcpy(m_current->ptr + nodePos, static_cast<const char*>(buf) + bufPos, size);
            if (nodeCap == size) {
                m_current = m_current->next;
            }
            m_position += size;
            bufPos = size;
            size = 0;
        }else {
            memcpy(m_current->ptr + nodePos, static_cast<const char*>(buf) + bufPos, nodeCap);
            m_position += nodeCap;
            bufPos += nodeCap;
            size -= nodeCap;
            m_current = m_current->next;
            nodeCap = m_nodeSize;
            nodePos = 0;
        }
    }

    // 更新m_size
    if (m_position > m_size) {
        m_size = m_position;
    }
}

void ByteArray::writeFInt8(int8_t value) {
    write(&value, sizeof(value));
}

void ByteArray::writeFUint8(uint8_t value) {
    write(&value, sizeof(value));
}

void ByteArray::writeFInt16(int16_t value) {
    writeFInt(value);
}

void ByteArray::writeFUint16(uint16_t value) {
    writeFInt(value);
}

void ByteArray::writeFInt32(int32_t value) {
    writeFInt(value);
}

void ByteArray::writeFUint32(uint32_t value) {
    writeFInt(value);
}

void ByteArray::writeFInt64(int64_t value) {
    writeFInt(value);
}

void ByteArray::writeFUint64(uint64_t value) {
    writeFInt(value);
}

// 下面四个函数分别是Zigzag的编码和解码
// 为varint编码做准备
// 将有符号整数编码为无符号整数
static uint32_t EncodeZigzag32(const int32_t& value) {
    // encoded = (n << 1) ^ (n >> 31)
    return (static_cast<uint32_t>(-value) << 1) ^ (value >> 31);
}

static uint64_t EncodeZigzag64(const int64_t& value) {
    // encoded = (n << 1) ^ (n >> 31)
    return (static_cast<uint64_t>(-value) << 1) ^ (value >> 63);
}

static int32_t DecodeZigzag32(const uint32_t& value) {
    // decoded = (encoded >> 1) ^ -(encoded & 1)
    return (value >> 1) ^ (-(value &1));
}

static int64_t DecodeZigzag64(const uint64_t& value) {
    // decoded = (encoded >> 1) ^ -(encoded & 1)
    return (value >> 1) ^ (-(value & 1));
}

void ByteArray::writeVInt32(int32_t value) {
    // 可变长度编码需要先将有符号数编码为无符号数，然后再进行varint编码
    // 所以这里先调用EncodeZigzag32()，将有符号数编码为无符号数
    // 然后再调用writeVUint32()，将无符号数进行varint编码
    writeVUint32(EncodeZigzag32(value));
}

void ByteArray::writeVUint32(uint32_t value) {
    uint8_t unitsArr[5]; // 创建一个包括5个Varint编码单元的数组
    int i=0;
    for (;i < 5;++i) {
        if (value >= 0x80) {
            unitsArr[i] = (value & 0x7F) | 0x80; // value & 0x7F每次取低位的7位，然后将最高位设置为1
        }else {
            unitsArr[i] = value & 0x7F; // 最后一个编码单元，最高位为0
            break;
        }
        value >> 7;
    }
    write(unitsArr, ++i);
}

void ByteArray::writeVInt64(int64_t value) {
    writeVUint64(EncodeZigzag64(value));
}

void ByteArray::writeVUint64(uint64_t value) {
    uint8_t unitsArr[10];
    int i = 0;
    for (;i < 10;++i) {
        if (value >= 0x80) {
            unitsArr[i] = (value & 0x7F) | 0x80;
        }else {
            unitsArr[i] = value & 0x7F;
            break;
        }
        value >>= 7;
    }
    write(unitsArr, ++i);
}

void ByteArray::writeFloat(float value) {
    // 将float类型的数据内存拷贝到uint32_t类型的内存空间，然后再调用writeFUint32()写入
    uint32_t tmp;
    memcpy(&tmp,&value,sizeof(value));
    writeFUint32(tmp);
}

void ByteArray::writeDouble(const double value) {
    // 将double类型的数据内存拷贝到uint64_t类型的内存空间，然后再调用writeFUint64()写入
    uint64_t tmp;
    memcpy(&tmp,&value,sizeof(value));
    writeFUint64(tmp);
}

void ByteArray::writeStringF16(const std::string& value) {
    // 写入字符串的长度
    writeFUint16(value.size());
    // 写入字符串
    write(value.c_str(),value.size()); // c_str()返回一个指向以\0结尾的字符数组的指针（C字符串指针）
}

void ByteArray::writeStringF32(const std::string& value) {
    writeFUint32(value.size());
    write(value.c_str(),value.size());
}

void ByteArray::writeStringF64(const std::string& value) {
    writeFUint64(value.size());
    write(value.c_str(),value.size());
}

void ByteArray::writeStringVint(const std::string& value) {
    writeVUint64(value.size());
    write(value.c_str(),value.size());
}

void ByteArray::writeStringWithoutLength(const std::string& value) {
    write(value.c_str(),value.size());
}

// 写入数据结束

// 读取数据
void ByteArray::read(void* buf, size_t size) {
    if (getReadableSize() < size) {
        SPDLOG_LOGGER_ERROR(logger,"no enough data to read");
        throw std::out_of_range("no enough data to read");
    }

    size_t nodePos = m_position % m_nodeSize;
    size_t bufPos = 0;
    size_t nodeCap = m_nodeSize - nodePos;

    while(size > 0)  {
        if (nodeCap >= size) {
            memcpy(static_cast<char*>(buf) + bufPos,m_current->ptr + nodePos, size);
            if (nodeCap == size) {
                m_current = m_current->next;
            }
            m_position += size;
            bufPos+=size;
            size = 0;
        }else {
            memcpy(static_cast<char*>(buf) + bufPos,m_current->ptr+nodePos,nodeCap);
            m_position+=nodeCap;
            bufPos += nodeCap;
            size-=nodeCap;
            m_current=m_current->next;
            nodePos = 0;
            nodeCap = m_nodeSize;
        }
            
    }
    
}

void ByteArray::read(void* buf, size_t size, size_t position) const {
    if (m_size < position + size) {
        SPDLOG_LOGGER_ERROR(logger,"no enough data to read");
        throw std::out_of_range("no enough data to read");
    }

    // ？原项目中没有进行这个实现，而我认为这个read的重载函数要实现的功能就是让用户指定读取的开始位置
    // ？那就势必要实现这个功能，也许原作者的想法是，即便由用户指定了读取的开始位置，也得大于m_position
    // ？根据position计算出需要从哪个node开始读取数据
    auto cur = m_head;
    size_t num = position / m_nodeSize;
    while(num--) {
        cur = cur->next;
    }

    size_t nodePos = position % m_nodeSize;
    size_t bufPos = 0;
    size_t nodeCap = m_nodeSize-nodePos;

    while(size > 0) {
        if (nodeCap >= size) {
            memcpy(static_cast<char*>(buf) + bufPos, cur->ptr + nodePos, size);
            size = 0;
        }else {
            memcpy(static_cast<char*>(buf) + bufPos, cur->ptr + nodePos, nodeCap);
            position += nodeCap;
            bufPos += nodeCap;
            size -= nodeCap;
            cur = cur->next;
            nodePos = 0;
        }
    }
}


int8_t ByteArray::readFInt8() {
    int8_t tmp;
    read(&tmp,sizeof(tmp));
    return tmp;
}

uint8_t ByteArray::readFUint8() {
    uint8_t tmp;
    read(&tmp,sizeof(tmp));
    return tmp;
}

#define readFInt(type)          \
    type tmp;                   \
    read(&tmp,sizeof(type));    \
    if (isLittleEndian()) {     \
        return ByteSwap(tmp);   \
    }                           \
    return tmp;


int16_t ByteArray::readFInt16() {
    readFInt(int16_t);
}

uint16_t ByteArray::readFUint16() {
    readFInt(uint16_t);
}

int32_t ByteArray::readFInt32() {
    readFInt(int32_t);
}

uint32_t ByteArray::readFUint32() {
    readFInt(uint32_t);
}

int64_t ByteArray::readFInt64() {
    readFInt(int64_t);
}

uint64_t ByteArray::readFUint64() {
    readFInt(uint64_t);
}

#undef readFInt

int32_t ByteArray::readVInt32() {
    // 由于有符号整数的编码是通过Zigzag编码得到的无符号整数，然后使用varint编码存储的
    // 所以这里需要先调用readVUint32()，解码varint得到无符号整数，然后再调用DecodeZigzag32()得到有符号整数
    return DecodeZigzag32(readVUint32());
}

uint32_t ByteArray::readVUint32() {
    // 每次读取一个字节的信息，判断这个字节的最高位是否为1，可知道是否还有下一个字节
    uint32_t result = 0;
    for (int i = 0;i < 32;i+=7) {
        uint8_t tmp = readFUint8();
        // 这里自然而然就转换为大端序
        if ((tmp >> 7) ==0) {
            result |= static_cast<uint32_t>(tmp) << i;
        }else {
            result |= static_cast<uint32_t>(tmp & 0x7F) << i;
        }
    }
    return result;
}

int64_t ByteArray::readVInt64() {
    return DecodeZigzag64(readVUint64());
}

uint64_t ByteArray::readVUint64() {
    uint64_t result = 0;
    for (int i = 0;i < 64;i+=7) {
        uint8_t tmp = readFUint8();
        if ((tmp >> 7) == 0) {
            result |= static_cast<uint64_t>(tmp) << i;
        }else {
            result |= static_cast<uint64_t>(tmp & 0x7F) << i;
        }
    }
    return result;
}

float ByteArray::readFloat() {
    // 与写入操作相反，先读取uint32_t类型的数据，然后再将其转换为float类型
    // 这里的转换也是单纯地将uint32_t类型的数据内存拷贝到float类型的内存空间
    uint32_t tmp = readFUint32();
    float result;
    memcpy(&result, &tmp, sizeof(tmp));
    return result;
}

double ByteArray::readDouble() {
    uint64_t tmp = readFUint64();
    double result;
    memcpy(&result, &tmp, sizeof(tmp));
    return result;
}

std::string ByteArray::readStringF16() {
    // 读取字符串的长度
    uint16_t len = readFUint16();
    std::string result;
    result.resize(len);
    read(&result[0],len);
    return result;
}

std::string ByteArray::readStringF32() {
    uint32_t len = readFUint32();
    std::string result;
    result.resize(len);
    read(&result[0],len);
    return result;
}

std::string ByteArray::readStringF64() {
    uint64_t len = readFUint64();
    std::string result;
    result.resize(len);
    read(&result[0],len);
    return result;
}

std::string ByteArray::readStringVint() {
    uint64_t len = readVUint64();
    std::string result;
    result.resize(len);
    read(&result[0],len);
    return result;
}

// 读取数据结束


// 文件的读写
bool ByteArray::writeToFile(const std::string& name) const{
    std::ofstream ofs(name, std::ios::trunc | std::ios::binary);
    if (!ofs) {
        SPDLOG_LOGGER_ERROR(logger,"Function writeToFile file={} failed, errno={}, errstr={}",name, errno, strerror(errno));
        return false;
    }

    auto readableSize = getReadableSize();
    Node* current = m_current;
    auto position = m_position;

    while(readableSize) {

        int offset = position % m_nodeSize;
        size_t len = (readableSize > m_nodeSize ? m_nodeSize : readableSize) - offset;
        ofs.write(current->ptr + offset, len);
        readableSize -= len;
        current = current->next;
        position += len;
    }
    return true;
}


bool ByteArray::readFromFile(const std::string& name) {
    std::ifstream ifs(name, std::ios::binary);
    if (ifs.fail()) {
        SPDLOG_LOGGER_ERROR(logger,"Function readFromFile file={} failed, errno={}, errstr={}",name, errno, strerror(errno));
        return false;
    }

    // 下面使用buffer来读取文件中的数据，然后再调用write()函数将buffer中的数据写入ByteArray中
    // 为什么不直接在调用read()函数时将m_current->ptr作为参数传入？
    // 因为无法保证m_capacity的大小，所以不能直接调用read()函数；如果m_capacity小于文件的大小，那么就会出现段错误
    // 而且byte_array是一个由node节点连接起来的链表，直接调用read函数，不会因为当前node节点写满而自动切换到下一个node节点
    std::shared_ptr<char> buffer(new char[m_nodeSize], [](char* ptr) { delete[] ptr;});
    // ifs.eof()用于判断文件是否读取到末尾
    // 也可以使用ifs.fail()来判断文件是否全部被读取出来，其中如果没有读取出来的数据的字节数，小于传入参数指定的字节数，也算读取失败。
    while(!ifs.eof()) {
        ifs.read(buffer.get(), m_nodeSize);
        write(buffer.get(), ifs.gcount()); // ifs.gcount()返回最后一次读取的字节数
    }
    return true;
}

// 文件的读写结束

// 将ByteArray中的数据转换为字符串，且不改变m_position的值
std::string ByteArray::toString() const {
    std::string str;
    if (!getReadableSize()) {
        return str;
    }
    str.resize(getReadableSize());
    // 由于此函数是const，所以不能调用非const的read函数
    read(&str[0],str.size(), m_position);
}


std::string ByteArray::toHexString() const {
    std::string str = toString();
    std::stringstream ss;

    for (int i = 0; i < str.size(); ++i) {
        if (i != 0 && i % 32 == 0) {
            ss << std::endl;
        }
        // setw()设置输出宽度，setfill()设置填充字符(即如果输出的字符不满足宽度时，需要在高位进行添加的字符)
        // hex表示以16进制输出，仅影响数值的输出格式，对于字符数据，它不会直接进行转换。
        ss << std::setw(2) << std::setfill('0') << std::hex << static_cast<int16_t>(static_cast<uint8_t>(str[i])) << " ";
    }
    return ss.str();
}

// 将ByteArray中的数据转换为字符串结束

// 将ByteArray中的数据读写入到iovec缓冲区集合中
uint64_t ByteArray::getReadBuffers(std::vector<iovec>& buffers, uint64_t len) const {
    len = len > getReadableSize() ? getReadableSize() : len;
    if (!len) {
        return 0;
    }

    auto size = len;
    
    size_t nodePos = m_position % m_nodeSize; // 当前node中的数据已经读取的字节数，或下一个要读取的字节下标
    Node* current = m_current;
    size_t nodeCap = m_size - nodePos; // 当前current指针所指向的node中剩余的字节数
    iovec iov;
    while(len) {
        if (nodeCap >= len) {
            iov.iov_base = current->ptr + nodePos;
            iov.iov_len = len;
            len = 0;
        }else {
            iov.iov_base = current->ptr + nodePos;
            iov.iov_len = nodeCap;
            len -= nodeCap;
            current = current->next;
            nodePos = 0;
            nodeCap = m_nodeSize;
        }
        buffers.push_back(iov);
    }
    return size;
}

uint64_t ByteArray::getReadBuffers(std::vector<iovec>& buffers, uint64_t len, uint64_t position) const {
    len = len > getReadableSize() ? getReadableSize() : len;
    if (!len) {
        return 0;
    }

    auto size = len;
    
    size_t nodePos = position % m_nodeSize; // 当前node中的数据已经读取的字节数，或下一个要读取的字节下标
    size_t nodeCap = m_size - nodePos; // 当前current指针所指向的node中剩余的字节数
    Node* current = m_head;
    auto count = position /m_nodeSize;

    // 将current指针指向position所在的node
    while(count--) {
        current = current->next;
    }

    iovec iov;
    while(len) {
        if (nodeCap >= len) {
            iov.iov_base = current->ptr + nodePos;
            iov.iov_len = len;
            len = 0;
        }else {
            iov.iov_base = current->ptr + nodePos;
            iov.iov_len = nodeCap;
            len -= nodeCap;
            current = current->next;
            nodePos = 0;
            nodeCap = m_nodeSize;
        }
        buffers.push_back(iov);
    }
    return size;
}

uint64_t ByteArray::getWriteBuffers(std::vector<iovec>& buffers, uint64_t len) {
    if (!len) {
        return 0;
    }
    auto size = len;
    addCapacity(len);

    size_t nodePos = m_position % m_nodeSize;
    size_t nodeCap = m_nodeSize - nodePos;
    Node* current = m_current;
    iovec iov;

    while(len) {
        if (nodeCap > len) {
            iov.iov_base = current->ptr + nodePos;
            iov.iov_len = len;
            len = 0;
        }else {
            iov.iov_base = current->ptr + nodePos;
            iov.iov_len = nodeCap;

            len-=nodeCap;
            current = current->next;
            nodePos=0;
            nodeCap = m_nodeSize;
        }
        buffers.push_back(iov);
    }

    return size;
}

// 将ByteArray中的数据读写入到iovec缓冲区集合中结束

bool ByteArray::isLittleEndian() const {
    return m_endian == std::endian::little;
}

void ByteArray::setLittleEndian() {
    if (!isLittleEndian()) {
        m_endian = std::endian::little;
    }
    
}

void ByteArray::setBigEndian() {
    if (isLittleEndian()) {
        m_endian = std::endian::big;
    }
}


size_t ByteArray::getReadableSize() const {
    return m_size - m_position;
}

size_t ByteArray::getPosition() const {
    return m_position;
}

void ByteArray::setPosition(size_t position) {
    if (position > m_capacity) {
        SPDLOG_LOGGER_ERROR(logger,"set position out of range");
        throw std::out_of_range("set position out of range");
    }
    m_position = position;
    if (m_position > m_size) {
        m_size = m_position;
    }
    m_current = m_head;
    while(position >= m_current->size) {
        position -= m_current->size;
        m_current = m_current->next;
    }

}

// 辅助功能函数

size_t ByteArray::getNodeSize() const {
    return m_nodeSize;
};

void ByteArray::clear() {
    m_position = m_size = 0;
    while(m_head->next) {
        Node* tmp = m_head->next;
        m_head->next = tmp->next;
        delete(tmp);
    }
    m_capacity = m_nodeSize;
    m_current = m_head;
    m_head->next = nullptr;
}

// private函数

size_t ByteArray::getAvailableCapacity() const {
    return m_capacity;
}

void ByteArray::addCapacity(size_t size) {
    auto availbleCap = getAvailableCapacity();
    // 如果剩余容量大于size，则不需要分配新的内存块
    if (availbleCap > size) {
        return;
    }

    // 计算需要分配的内存块数量
    auto newCap = size-availbleCap;
    auto nodeNum = ceil(1.0 * newCap / m_nodeSize); // ceil用于将newCap向上取整
    
    // 分配nodeNum个内存块，并且将这些内存块连接到链表中
    Node* tmp = m_head;
    while(tmp->next) {
        tmp = tmp->next;
    }
    Node* first = nullptr;
    for (int i = 0;i < nodeNum;++i) {
        tmp->next = new Node(m_nodeSize);
        if (!first) {
            first = tmp->next;
        }
        tmp = tmp->next;
        m_capacity +=1*m_nodeSize;
    }

    // 如果在分配新的node之前，m_current指向的node已经满了
    // 则需要在分配完node之后，将m_current指向first（第一个新分配的node）
    if (availbleCap == 0) {
        m_current = first;
    }
}

} // namespace RR
