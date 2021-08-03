//
//  sio_message.h
//
//  Created by Melo Yao on 3/25/15.
//

#ifndef __SIO_MESSAGE_H__
#define __SIO_MESSAGE_H__
#include <string>
#include <memory>
#include <vector>
#include <map>
#include <cassert>
#include <type_traits>

#ifdef SIO_DLL
#ifdef _WIN32
#ifdef SIO_EXPORT
#define SIO_API __declspec(dllexport)
#else
#define SIO_API __declspec(dllimport)
#endif
#else
#define SIO_API __attribute__((visibility("default")))
#endif
#else
#define SIO_API
#endif

namespace sio
{
    class SIO_API MapStr {
        typedef std::map<std::string, std::string> Container;
        Container map_;
    public:
        MapStr() {}
        Container& map();
        const Container& map() const;

        typedef bool(*EnumFunc)(const char* k, const char* v, void* data);
        void enumerate(EnumFunc f, void* data) const;
        int size() const;
        void clear() { map_.clear(); }
        const char* get(const char* key) const;
        bool has(const char* key) const;
        bool del(const char* key);
        bool set(const char* key, const char* value);
    };

    class message;
    typedef std::shared_ptr<message> msgptr;
    class SIO_API MapMsg {
        typedef std::map<std::string, msgptr> Container;
        Container map_;
    public:
        MapMsg();
        Container& map();
        const Container& map() const;

        typedef bool(*EnumFunc)(const char* k, msgptr p, void* data);
        void enumerate(EnumFunc f, void* data) const;
        int size() const;
        void clear() { map_.clear(); }
        msgptr get(const char* key) const;
        bool has(const char* key) const;
        bool del(const char* key);
        bool set(const char* key, msgptr value);
    };

    class SIO_API message
    {
    public:
        enum flag
        {
            flag_integer,
            flag_double,
            flag_string,
            flag_binary,
            flag_array,
            flag_object,
            flag_boolean,
            flag_null
        };

        virtual ~message(){};

        class list;

        flag get_flag() const
        {
            return _flag;
        }

        typedef std::shared_ptr<message> ptr;

        virtual bool get_bool() const
        {
            assert(false);
            return false;
        }

        virtual int64_t get_int() const
        {
            assert(false);
            return 0;
        }

        virtual double get_double() const
        {
            assert(false);
            return 0;
        }

        virtual std::string const& get_string() const
        {
            assert(false);
            static std::string s_empty_string;
            return s_empty_string;
        }

        virtual std::shared_ptr<const std::string> const& get_binary() const
        {
            assert(false);
            return nullptr;
        }

        virtual const std::vector<ptr>& get_vector() const
        {
            assert(false);
            static std::vector<ptr> s_empty_vector;
            s_empty_vector.clear();
            return s_empty_vector;
        }

        virtual std::vector<ptr>& get_vector()
        {
            assert(false);
            static std::vector<ptr> s_empty_vector;
            s_empty_vector.clear();
            return s_empty_vector;
        }

        virtual const MapMsg& get_map() const
        {
            assert(false);
            static MapMsg s_empty_map;
            return s_empty_map;
        }

        virtual MapMsg& get_map()
        {
            assert(false);
            static MapMsg s_empty_map;
            return s_empty_map;
        }
    private:
        flag _flag;

    protected:
        message(flag f):_flag(f){}
    };

    class SIO_API null_message : public message
    {
    protected:
        null_message()
            :message(flag_null)
        {
        }

    public:
        static message::ptr create()
        {
            return ptr(new null_message());
        }
    };

    class SIO_API bool_message : public message
    {
        bool _v;

    protected:
        bool_message(bool v)
            :message(flag_boolean),_v(v)
        {
        }

    public:
        static message::ptr create(bool v)
        {
            return ptr(new bool_message(v));
        }

        bool get_bool() const
        {
            return _v;
        }
    };

    class SIO_API int_message : public message
    {
        int64_t _v;
    protected:
        int_message(int64_t v)
            :message(flag_integer),_v(v)
        {
        }

    public:
        static message::ptr create(int64_t v)
        {
            return ptr(new int_message(v));
        }

        int64_t get_int() const
        {
            return _v;
        }

        double get_double() const//add double accessor for integer.
        {
            return static_cast<double>(_v);
        }
    };

    class SIO_API double_message : public message
    {
        double _v;
        double_message(double v)
            :message(flag_double),_v(v)
        {
        }

    public:
        static message::ptr create(double v)
        {
            return ptr(new double_message(v));
        }

        double get_double() const
        {
            return _v;
        }
    };

    class SIO_API string_message : public message
    {
        std::string _v;
        string_message(std::string const& v)
            :message(flag_string),_v(v)
        {
        }

        string_message(const char* v)
            :message(flag_string),_v(v)
        {
        }
    public:
        static message::ptr create(std::string const& v)
        {
            return ptr(new string_message(v));
        }

        static message::ptr create(const char* v)
        {
            return ptr(new string_message(v));
        }

        const char* c_str() const
        {
            return _v.c_str();
        }

        std::string const& get_string() const
        {
            return _v;
        }
    };

    class SIO_API binary_message : public message
    {
        std::shared_ptr<const std::string> _v;
        binary_message(std::shared_ptr<const std::string> const& v)
            :message(flag_binary),_v(v)
        {
        }
    public:
        static message::ptr create(std::shared_ptr<const std::string> const& v)
        {
            return ptr(new binary_message(v));
        }

        static message::ptr create(const uint8_t* data, int size)
        {
            auto v = std::make_shared<std::string>((const char*)data, size);
            return ptr(new binary_message(v));
        }

        void set(const uint8_t* data, int size) {
            _v = std::make_shared<std::string>((const char*)data, size);
        }
        uint8_t* get_binary(int& size) const {
            size = _v->length();
            return (uint8_t*)_v->data();
        }

        std::shared_ptr<const std::string> const& get_binary() const
        {
            return _v;
        }
    };

    class SIO_API array_message : public message
    {
        std::vector<message::ptr> _v;
        array_message():message(flag_array)
        {
        }

    public:
        static message::ptr create()
        {
            return ptr(new array_message());
        }

        void push(message::ptr const& message)
        {
            if(message)
                _v.push_back(message);
        }

        void push(const std::string& text)
        {
            _v.push_back(string_message::create(text));
        }

        void push(const char* text)
        {
            _v.push_back(string_message::create(text));
        }

        void push(std::shared_ptr<std::string> const& binary)
        {
            if(binary)
                _v.push_back(binary_message::create(binary));
        }

        void push(std::shared_ptr<const std::string> const& binary)
        {
            if(binary)
                _v.push_back(binary_message::create(binary));
        }

        void insert(size_t pos,message::ptr const& message)
        {
            _v.insert(_v.begin()+pos, message);
        }

        void insert(size_t pos,const std::string& text)
        {
            _v.insert(_v.begin()+pos, string_message::create(text));
        }

        void insert(size_t pos,const char* text)
        {
            _v.insert(_v.begin()+pos, string_message::create(text));
        }

        void insert(size_t pos,std::shared_ptr<std::string> const& binary)
        {
            if(binary)
                _v.insert(_v.begin()+pos, binary_message::create(binary));
        }

        void insert(size_t pos,std::shared_ptr<const std::string> const& binary)
        {
            if(binary)
                _v.insert(_v.begin()+pos, binary_message::create(binary));
        }

        size_t size() const
        {
            return _v.size();
        }

        const message::ptr& at(size_t i) const
        {
            return _v[i];
        }

        const message::ptr& operator[] (size_t i) const
        {
            return _v[i];
        }

        std::vector<ptr>& get_vector()
        {
            return _v;
        }

        const std::vector<ptr>& get_vector() const
        {
            return _v;
        }
    };

    class SIO_API object_message : public message
    {
        MapMsg _v;
        object_message() : message(flag_object)
        {
        }
    public:
        static message::ptr create()
        {
            return ptr(new object_message());
        }

        void insert(const char* key, message::ptr const& message)
        {
            _v.set(key, message);
        }

        void insert(const char* key,const std::string& text)
        {
            _v.set(key, string_message::create(text));
        }

        void insert(const char* key,const char* text)
        {
            _v.set(key, string_message::create(text));
        }

        void insert(const char* key,std::shared_ptr<std::string> const& binary)
        {
            if(binary)
                _v.set(key, binary_message::create(binary));
        }

        void insert(const char* key,std::shared_ptr<const std::string> const& binary)
        {
            if(binary)
                _v.set(key, binary_message::create(binary));
        }

        bool has(const char* key)
        {
            return _v.has(key);
        }

        const message::ptr& at(const char* key) const
        {
            return _v.get(key);
        }

        const message::ptr& operator[] (const char* key) const
        {
            return at(key);
        }

        bool has(const char* key) const
        {
            return _v.has(key);
        }

        MapMsg& get_map()
        {
            return _v;
        }

        const MapMsg& get_map() const
        {
            return _v;
        }
    };

    class SIO_API message::list
    {
    public:
        list()
        {
        }

        list(std::nullptr_t)
        {
        }

        list(message::list&& rhs):
            m_vector(std::move(rhs.m_vector))
        {

        }

        list & operator= (const message::list && rhs)
        {
            m_vector = std::move(rhs.m_vector);
            return *this;
        }

        template <typename T>
        list(T&& content,
            typename std::enable_if<std::is_same<std::vector<message::ptr>,typename std::remove_reference<T>::type>::value>::type* = 0):
            m_vector(std::forward<T>(content))
        {
        }

        list(message::list const& rhs):
            m_vector(rhs.m_vector)
        {

        }

        list(message::ptr const& message)
        {
            if(message)
                m_vector.push_back(message);

        }

        list(const std::string& text)
        {
            m_vector.push_back(string_message::create(text));
        }

        list(const char* text)
        {
            m_vector.push_back(string_message::create(text));
        }

        list(std::shared_ptr<std::string> const& binary)
        {
            if(binary)
                m_vector.push_back(binary_message::create(binary));
        }

        list(std::shared_ptr<const std::string> const& binary)
        {
            if(binary)
                m_vector.push_back(binary_message::create(binary));
        }

        void push(message::ptr const& message)
        {
            if(message)
                m_vector.push_back(message);
        }

        void push(const std::string& text)
        {
            m_vector.push_back(string_message::create(text));
        }

        void push(const char* text)
        {
            m_vector.push_back(string_message::create(text));
        }

        void push(std::shared_ptr<std::string> const& binary)
        {
            if(binary)
                m_vector.push_back(binary_message::create(binary));
        }

        void push(std::shared_ptr<const std::string> const& binary)
        {
            if(binary)
                m_vector.push_back(binary_message::create(binary));
        }

        void insert(size_t pos,message::ptr const& message)
        {
            m_vector.insert(m_vector.begin()+pos, message);
        }

        void insert(size_t pos,const std::string& text)
        {
            m_vector.insert(m_vector.begin()+pos, string_message::create(text));
        }

        void insert(size_t pos,const char* text)
        {
            m_vector.insert(m_vector.begin()+pos, string_message::create(text));
        }

        void insert(size_t pos,std::shared_ptr<std::string> const& binary)
        {
            if(binary)
                m_vector.insert(m_vector.begin()+pos, binary_message::create(binary));
        }

        void insert(size_t pos,std::shared_ptr<const std::string> const& binary)
        {
            if(binary)
                m_vector.insert(m_vector.begin()+pos, binary_message::create(binary));
        }

        size_t size() const
        {
            return m_vector.size();
        }

        const message::ptr& at(size_t i) const
        {
            return m_vector[i];
        }

        const message::ptr& operator[] (size_t i) const
        {
            return m_vector[i];
        }

        message::ptr to_array_message(const char* event_name) const
        {
            message::ptr arr = array_message::create();
            arr->get_vector().push_back(string_message::create(event_name));
            arr->get_vector().insert(arr->get_vector().end(),m_vector.begin(),m_vector.end());
            return arr;
        }

        message::ptr to_array_message() const
        {
            message::ptr arr = array_message::create();
            arr->get_vector().insert(arr->get_vector().end(),m_vector.begin(),m_vector.end());
            return arr;
        }

    private:
        std::vector<message::ptr> m_vector;
    };
}

#endif
