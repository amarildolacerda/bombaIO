#include "texts.h"

namespace Texts
{

    String leftPad(String original, int length, char fillChar)
    {
        // Verifica se precisa truncar
        if (original.length() >= length)
        {
            return original.substring(0, length);
        }

        String result;
        result.reserve(length); // Pré-aloca memória

        // Adiciona preenchimento
        for (int i = 0; i < length - original.length(); ++i)
        {
            result += fillChar;
        }

        // Adiciona texto original
        result += original;

        return result;
    }

    String centerText(String text, int width, char fillChar)
    {
        const int textLength = text.length();

        // Verifica se precisa truncar
        if (textLength >= width)
        {
            return text.substring(0, width);
        }

        String centered;
        centered.reserve(width); // Pré-aloca memória

        const int totalPadding = width - textLength;
        const int leftPadding = totalPadding / 2;
        const int rightPadding = totalPadding - leftPadding;

        // Preenchimento esquerdo
        for (int i = 0; i < leftPadding; ++i)
        {
            centered += fillChar;
        }

        // Texto central
        centered += text;

        // Preenchimento direito
        for (int i = 0; i < rightPadding; ++i)
        {
            centered += fillChar;
        }

        return centered;
    }

} // namespace Texts