#include <iostream>
#include <stdexcept>
#include <algorithm> // para std::swap

class Vector {
private:
    int* data,
         m_size,      // cantidad actual
         m_capacity;  // capacidad

    void reserve(int new_cap) {
        if (new_cap <= m_capacity) return;
        int* new_data = new int[new_cap];
        for (int i = 0; i < m_size; ++i) {
            new_data[i] = data[i];
        }
        delete[] data;
        data = new_data;
        m_capacity = new_cap;
    }

public:
    Vector() : data(nullptr), m_size(0), m_capacity(0) {}

    ~Vector() {
        delete[] data;
    }

    Vector(const Vector& other) : data(nullptr), m_size(0), m_capacity(0) {
        reserve(other.m_size);
        for (int i = 0; i < other.m_size; ++i) {
            data[i] = other.data[i];
        }
        m_size = other.m_size;
    }

    Vector& operator=(Vector other) {
        swap(other);
        return *this;
    }

    Vector(Vector&& other) noexcept : data(nullptr), m_size(0), m_capacity(0) {
        swap(other);
    }

    Vector& operator=(Vector&& other) noexcept {
        swap(other);
        return *this;
    }

    void swap(Vector& other) noexcept {
        std::swap(data, other.data);
        std::swap(m_size, other.m_size);
        std::swap(m_capacity, other.m_capacity);
    }

    void push_back(int value) {
        if (m_size == m_capacity) {
            int new_cap = (m_capacity == 0) ? 1 : m_capacity * 2;
            reserve(new_cap);
        }
        data[m_size] = value;
        ++m_size;
    }

    void pop_back() {
        if (m_size > 0) {
            --m_size;
            // data[m_size].~int();
        }
    }

    int& operator[](int index) {
        return data[index];
    }

    const int& operator[](int index) const {
        return data[index];
    }

    int& at(int index) {
        if (index >= m_size) throw std::out_of_range("Indice fuera de rango");
        return data[index];
    }

    const int& at(int index) const {
        if (index >= m_size) throw std::out_of_range("Indice fuera de rango");
        return data[index];
    }

    int size() const { return m_size; }
    int capacity() const { return m_capacity; }
    bool empty() const { return m_size == 0; }

    void clear() {
        for (int i = 0; i < m_size; ++i) {
            // data[i].~int();
        }
        m_size = 0;
    }
};