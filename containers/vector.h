#ifndef __VECTOR_H__
#define __VECTOR_H__

#include <iostream>
#include <stdexcept>
#include <algorithm> // para std::swap

using T = int;
class Vector {
private:
    T  * data,
         m_size,      // cantidad actual
         m_capacity;  // capacidad

    void reserve(int new_cap) {
        if (new_cap <= m_capacity) return;
        T* new_data = new T[new_cap];
        for (size_t i = 0; i < m_size; ++i) {
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
        for (size_t i = 0; i < other.m_size; ++i) {
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

    void push_back(T value) {
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
            // data[m_size].~T();
        }
    }

    T& operator[](int index) {
        return data[index];
    }

    const T& operator[](int index) const {
        return data[index];
    }

    T& at(int index) {
        if (index >= m_size) throw std::out_of_range("Indice fuera de rango");
        return data[index];
    }

    const T& at(int index) const {
        if (index >= m_size) throw std::out_of_range("Indice fuera de rango");
        return data[index];
    }

    T size() const { return m_size; }
    T capacity() const { return m_capacity; }
    bool empty() const { return m_size == 0; }

    void clear() {
        for (size_t i = 0; i < m_size; ++i) {
            // data[i].~T();
        }
        m_size = 0;
    }
};

#endif // __VECTOR_H__