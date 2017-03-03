#include <string>
#include <iostream>
#include <google/protobuf/descriptor.h>
#include <google/protobuf/message.h>

#include <functional>
#include <map>
#include <memory>
#include <query.pb.h>
#include <netinet/in.h>
#include <zlib.h>

//4个字节的长度
constexpr int headerLen = sizeof(int32_t);

//根据type_name反射protobuf message
google::protobuf::Message* createMessage(const std::string& type_name) {
    google::protobuf::Message* message = nullptr;
    const google::protobuf::Descriptor* descriptor =
            google::protobuf::DescriptorPool::generated_pool()->FindMessageTypeByName(type_name);

    if (descriptor) {
        const google::protobuf::Message* prototype =
                google::protobuf::MessageFactory::generated_factory()->GetPrototype(descriptor);
        if (prototype) {
            message = prototype->New();
        }
    }

    return message;
}

//回调函数基类
class Callback {
public:
    virtual ~Callback() {};

    //当不同pb消息到达时触发相应的业务逻辑
    virtual void onMessage(google::protobuf::Message* message) const = 0;
};

//模板类，可以用不同的pb消息作为参数
template<typename T>
class CallbackT: public Callback {
public:
    using ProtobufMessageCallback = std::function<void(T* message)>;

    CallbackT(const ProtobufMessageCallback& callback)
            :callback_ (callback)
    {}

    //实现业务逻辑的接口
    virtual void onMessage(google::protobuf::Message* message) const {
        //进行下行类型转换
        T* t = dynamic_cast<T*>(message);

        //执行回调函数
        callback_(t);
    }

private:
    ProtobufMessageCallback callback_;
};

//业务分发器
class ProtobufDispatcher {
public:
    ProtobufDispatcher() {};

    void onMessage(google::protobuf::Message* message) const {
        auto it = callbacks_.find(message->GetDescriptor());
        if (it != callbacks_.end()) {
            it->second->onMessage(message);
        } else {

        }
    }

    //在此处，将每个pb消息的唯一descriptor与执行函数进行映射
    template<typename T>
    void registerMessageCallback(const typename CallbackT<T>::ProtobufMessageCallback& callback) {
        callbacks_[T::descriptor()] =  std::make_shared<CallbackT<T>>(callback);
    }

private:
    using CallbackMap = std::map<const google::protobuf::Descriptor*, std::shared_ptr<Callback>>;
    CallbackMap callbacks_;
};

void onQuery(msg::Query* query) {
    std::cout << "onQuery: " << query->GetTypeName() << std::endl;
}

void onAnswer(msg::Answer* answer) {
    std::cout << "onAnswer: " << answer->GetTypeName() << std::endl;
}

std::string encode(const google::protobuf::Message& message) {
    std::string result;
    result.resize(headerLen);

    const std::string& typeName = message.GetTypeName();
    int32_t nameLen = static_cast<int32_t>(typeName.size() + 1);
    int32_t be32 = ::htonl(nameLen);
    result.append(reinterpret_cast<char*>(&be32), sizeof be32);
    result.append(typeName.c_str(), nameLen);
    bool succeed = message.AppendToString(&result);

    if (succeed) {
        const char* begin = result.c_str() + headerLen;
        int32_t checkSum = adler32(1, reinterpret_cast<const Bytef*>(begin), result.size() - headerLen);
        int32_t be32 = ::htonl(checkSum);
        result.append(reinterpret_cast<char*>(&be32), sizeof be32);
        int32_t len = ::htonl(result.size() - headerLen);
        std::copy(reinterpret_cast<char*>(&len),
                  reinterpret_cast<char*>(&len) + sizeof len,
                  result.begin());
    } else {
        result.clear();
    }

    return result;
}

int32_t asInt32(const char* buf) {
    int32_t be32 = 0;
    ::memcpy(&be32, buf, sizeof be32);
    return ::ntohl(be32);
}

google::protobuf::Message* decode(const std::string& buf) {
    google::protobuf::Message* result = nullptr;
    int len = static_cast<int32_t>(buf.size());
    int32_t expectedCheckSum = asInt32(buf.c_str() + len - headerLen);
    const char* begin = buf.c_str();
    int32_t checkSum = adler32(1, reinterpret_cast<const Bytef*>(begin), len - headerLen);
    if (checkSum == expectedCheckSum) {
        int32_t nameLen = asInt32(buf.c_str());
        if (nameLen != 0) {
            std::string typeName(buf.begin() + headerLen, buf.begin() + headerLen + nameLen - 1);
            google::protobuf::Message* message = createMessage(typeName);
            if (message) {
                const char* data = buf.c_str() + headerLen + nameLen;
                int32_t dataLen = len - nameLen - 2 * headerLen;
                if (message->ParseFromArray(data, dataLen)) {
                    result = message;
                } else { //if (message->ParseFromArray(data, dataLen)) {
                    delete message;
                }
            } else { //if (message)

            }
        } else { //if (nameLen != 0) {

        }
    } else { //if (checkSum == expectedCheckSum)

    }

    return result;
}

int main() {
    msg::Query q;
    msg::Answer a;
    q.set_id(17037);
    q.set_questioner("hello?");

    //模拟发送端编码
    auto eq = encode(q);

    //模拟接受端解码
    //先取出长度
    int32_t len = 0;
    std::copy(eq.begin(), eq.begin() + headerLen, reinterpret_cast<char*>(&len));
    len = ::ntohl(len);
    eq.erase(eq.begin(), eq.begin() + headerLen);

    //解码
    auto dq = decode(eq);
    dq = dynamic_cast<msg::Query*>(dq);

    //在消息处理中心注册消息分发路由
    ProtobufDispatcher dispatcher;
    dispatcher.registerMessageCallback<msg::Query>(onQuery);
    dispatcher.registerMessageCallback<msg::Answer>(onAnswer);

    //模拟消息分发
    dispatcher.onMessage(&q);
    dispatcher.onMessage(&a);
    return 0;
}