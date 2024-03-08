#include "byte_array.h"



namespace RR {

static auto logger = GetLoggerInstance();
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
ByteArray::Node::Node(size_t size) : ptr(new char[size]), next(nullptr), size(size) {}

ByteArray::Node::~Node() {
    if (ptr) {
        delete[] ptr;
    }
    ptr = nullptr;
}

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
    uint32_t tmp;
    memcpy(&tmp,&value,sizeof(value));
    writeFUint32(tmp);
}

void ByteArray::writeDouble(const double value) {
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


// 读取数据
void ByteArray::read(void* buf, size_t size) {
    if (getReadableSize() < size) {
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
        throw std::out_of_range("no enough data to read");
    }

    // ? 原项目中没有进行这个实现，而我认为这个read的重载函数要实现的功能就是让用户指定读取的开始位置
    // 那就势必要实现这个功能，也许原作者的想法是，即便由用户指定了读取的开始位置，也得大于m_position
    // 根据position计算出需要从哪个node开始读取数据
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

size_t ByteArray::getReadableSize() const {
    return m_size - m_position;
}

size_t ByteArray::getPosition() const {
    return m_position;
}

size_t ByteArray::getNodeSize() const {
    return m_nodeSize;
};

} // namespace RR
