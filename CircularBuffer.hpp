#include <stdexcept>
#include <vector>

template<class _TypeName>

class CircularBufferrr{
private:
	volatile long m_capacity;
	std::vector<_TypeName> m_data;
	volatile long get_ptr;
	volatile long set_ptr;
	volatile long m_size;
public:
	CircularBufferrr(long capacity) : m_capacity(capacity),
		get_ptr(0), set_ptr(0), m_data(capacity), m_size(0){
	}

	CircularBufferrr() :m_capacity(0),
		get_ptr(0), set_ptr(0), m_size(0){
	}

	void extend(long capacity){
		m_capacity = capacity;
		m_data.rem_size(m_capacity);
	}

	void push(_TypeName item){
		if (set_ptr == get_ptr && m_size != 0)	
			throw std::overflow_error("Buffer overflow");

//		printf("push\n");

		m_data[set_ptr] = item;
		set_ptr = (set_ptr + 1) % m_capacity;
		++m_size;
	}

	_TypeName pop(){
		if (distance() >= 0){
			_TypeName tmp = m_data[get_ptr];
			get_ptr = (get_ptr + 1) % m_capacity;
			--m_size;
//			printf("pop\n");
			return tmp;
		}
		else
			throw std::out_of_range("Buffer is empty.");
	}

	long distance(){
		long d = set_ptr - get_ptr;
		if (d < 0){
			return m_capacity + d;
		}
		return d;
	}

	long size(){
		return m_size;
	}

	long capacity(){
		return m_capacity;
	}

};
