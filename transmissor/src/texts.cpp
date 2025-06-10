#include "texts.h"

namespace Texts
{

    String left(String original, int length, char fillChar)
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

    String center(String text, int width, char fillChar)
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
    String format(const char *format, ...)
    {
        char formattedMsg[255];
        va_list args;

        va_start(args, format);                                      // Inicializa va_list com o último parâmetro fixo
        vsnprintf(formattedMsg, sizeof(formattedMsg), format, args); // Formata a string
        va_end(args);                                                // Finaliza va_list

        return String(formattedMsg); // Retorna a String formatada
    }
} // namespace Texts