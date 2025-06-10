#ifndef TEXTS_H
#define TEXTS_H

#include <Arduino.h>

namespace Texts
{

    /**
     * @brief Alinha texto à esquerda com preenchimento
     * @param original Texto original
     * @param length Comprimento total desejado
     * @param fillChar Caractere de preenchimento (padrão: espaço)
     * @return String formatada
     */
    String leftPad(String original, int length, char fillChar = ' ');

    /**
     * @brief Centraliza texto com preenchimento
     * @param text Texto original
     * @param width Largura total desejada
     * @param fillChar Caractere de preenchimento (padrão: espaço)
     * @return String centralizada
     */
    String centerText(String text, int width, char fillChar = ' ');

} // namespace Texts

#endif // TEXTS_H