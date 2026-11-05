
#ifndef __TREEPRINTER_H__
#define __TREEPRINTER_H__

#include "../types.h"
#include "../utilities/printcolored.h"
#include <algorithm>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

template <typename Node>
class TreePrinter {
public:
    using LinkPtr = Node*;

private:
    struct Posicion {
        LinkPtr nodo;
        TX columna;
        TX nivel;
        std::string texto;
    };

    LinkPtr m_root;

    void recolectar(
        LinkPtr nodo, TX nivel, TX& orden,
        std::vector<Posicion>& posiciones,
        TX& longitud
    ) const {
        if (!nodo) return;

        recolectar(
            nodo->m_pChild[0],
            nivel + 1, orden,
            posiciones, longitud
        );

        std::ostringstream texto;
        texto << nodo->getValue();

        std::string valor = texto.str();

        longitud = std::max(
            longitud,
            static_cast<TX>(valor.size())
        );

        posiciones.push_back({
            nodo, orden++, nivel, valor
        });

        recolectar(
            nodo->m_pChild[1],
            nivel + 1, orden,
            posiciones, longitud
        );
    }

    TX indice(
        LinkPtr nodo,
        const std::vector<Posicion>& posiciones
    ) const {
        for (TX i = 0;
             i < static_cast<TX>(posiciones.size());
             ++i) {

            if (posiciones[i].nodo == nodo)
                return i;
        }

        return -1;
    }

    Color color(LinkPtr nodo) const {
        if (nodo == m_root)
            return Color::Green;

        if (!nodo->m_pChild[0] &&
            !nodo->m_pChild[1])
            return Color::Red;

        return Color::Yellow;
    }

    void de_lado(
        std::ostream& os,
        LinkPtr nodo,
        TX nivel
    ) const {
        if (!nodo) return;

        de_lado(os, nodo->m_pChild[1], nivel + 1);

        for (TX i = 0; i < nivel; ++i)
            os << "    ";

        PrintColored(
            os, nodo->getValue(), color(nodo)
        );

        de_lado(os, nodo->m_pChild[0], nivel + 1);
    }

public:
    explicit TreePrinter(LinkPtr raiz)
        : m_root(raiz) {}

    void printSide(
        std::ostream& os = std::cout
    ) const {
        if (!m_root) {
            os << "Arbol vacio\n";
            return;
        }

        de_lado(os, m_root, 0);
    }

    void print(
        std::ostream& os = std::cout
    ) const {
        if (!m_root) {
            os << "Arbol vacio\n";
            return;
        }

        std::vector<Posicion> posiciones;

        TX orden = 0;
        TX longitud = 0;
        TX nivel_max = 0;

        recolectar(
            m_root, 0, orden,
            posiciones, longitud
        );

        TX espacio = std::max(
            TX(4), longitud + 2
        );

        for (auto& p : posiciones) {
            p.columna = p.columna * espacio + espacio;

            nivel_max = std::max(
                nivel_max, p.nivel
            );
        }

        std::vector<TX> salto(nivel_max, 2);

        for (const auto& p : posiciones) {
            for (TX lado = 0; lado < 2; ++lado) {
                LinkPtr hijo = p.nodo->m_pChild[lado];

                if (!hijo) continue;

                TX destino = indice(hijo, posiciones);

                TX distancia =
                    posiciones[destino].columna
                    - p.columna;

                if (distancia < 0)
                    distancia = -distancia;

                salto[p.nivel] = std::max(
                    salto[p.nivel], distancia
                );
            }
        }

        std::vector<TX> fila_nivel(
            nivel_max + 1, 0
        );

        for (TX i = 1; i <= nivel_max; ++i) {
            fila_nivel[i] =
                fila_nivel[i - 1] + salto[i - 1];
        }

        TX filas = fila_nivel[nivel_max] + 1;
        TX ancho = (orden + 2) * espacio;

        if (static_cast<TXX>(filas) * ancho
            > TXX(2000000)) {

            os << "(Visualizacion lateral)\n";
            printSide(os);
            return;
        }

        std::vector<std::string> lienzo(
            filas, std::string(ancho, ' ')
        );

        std::vector<std::vector<Color>> colores(
            filas,
            std::vector<Color>(ancho, Color::Reset)
        );

        // Dibujar conexiones 
        for (const auto& p : posiciones) {
            for (TX lado = 0; lado < 2; ++lado) {
                LinkPtr hijo = p.nodo->m_pChild[lado];

                if (!hijo) continue;

                TX destino = indice(hijo, posiciones);

                TX dx =
                    posiciones[destino].columna
                    - p.columna;

                TX y1 = fila_nivel[p.nivel];

                TX alto =
                    fila_nivel[posiciones[destino].nivel]
                    - y1;

                for (TX paso = 1;
                     paso < alto;
                     ++paso) {

                    TX x = p.columna
                         + dx * paso / alto;

                    TX y = y1 + paso;

                    if (x >= 0 && x < ancho &&
                        y >= 0 && y < filas) {

                        lienzo[y][x] =
                            lado == 0 ? '/' : '\\';
                    }
                }
            }
        }

        // colores
        for (const auto& p : posiciones) {
            TX y = fila_nivel[p.nivel];

            TX inicio = p.columna
                - static_cast<TX>(p.texto.size()) / 2;

            for (TX i = 0;
                 i < static_cast<TX>(p.texto.size());
                 ++i) {

                TX x = inicio + i;

                if (x >= 0 && x < ancho &&
                    y >= 0 && y < filas) {

                    lienzo[y][x] = p.texto[i];
                    colores[y][x] = color(p.nodo);
                }
            }
        }

        // Dibujar
        for (TX y = 0; y < filas; ++y) {
            TX ultimo = ancho - 1;

            while (ultimo >= 0 &&
                   lienzo[y][ultimo] == ' ')
                --ultimo;

            TX x = 0;

            while (x <= ultimo) {
                Color c = colores[y][x];
                std::string parte;

                while (x <= ultimo &&
                       colores[y][x] == c) {

                    parte += lienzo[y][x++];
                }

                if (c == Color::Reset)
                    os << parte;
                else
                    PrintColored(
                        os, parte, c, false
                    );
            }

            os << '\n';
        }
    }
};

#endif
