static bool af_project_image_token_character(unsigned char byte) {
    return isalnum(byte) || byte == '_' || byte == '-' || byte == '.' || byte == '/';
}

static bool af_project_image_encode_token(const char *input, char *output, size_t capacity) {
    static const char hex[] = "0123456789ABCDEF";
    size_t write = 0U;
    if (input == NULL || output == NULL || capacity == 0U) {
        return false;
    }
    for (size_t index = 0U; input[index] != '\0'; ++index) {
        const unsigned char byte = (unsigned char)input[index];
        if (af_project_image_token_character(byte)) {
            if (write + 1U >= capacity) {
                return false;
            }
            output[write++] = (char)byte;
        } else {
            if (write + 3U >= capacity) {
                return false;
            }
            output[write++] = '%';
            output[write++] = hex[byte >> 4U];
            output[write++] = hex[byte & 15U];
        }
    }
    output[write] = '\0';
    return write != 0U;
}

static bool af_project_image_hex(char character, uint8_t *out_value) {
    if (character >= '0' && character <= '9') {
        *out_value = (uint8_t)(character - '0');
    } else if (character >= 'A' && character <= 'F') {
        *out_value = (uint8_t)(character - 'A' + 10);
    } else if (character >= 'a' && character <= 'f') {
        *out_value = (uint8_t)(character - 'a' + 10);
    } else {
        return false;
    }
    return true;
}

static bool af_project_image_decode_token(const char *input, char *output, size_t capacity) {
    size_t write = 0U;
    if (input == NULL || output == NULL || capacity == 0U) {
        return false;
    }
    for (size_t index = 0U; input[index] != '\0'; ++index) {
        unsigned char byte = (unsigned char)input[index];
        if (byte == '%') {
            uint8_t high = 0U;
            uint8_t low = 0U;
            if (input[index + 1U] == '\0' || input[index + 2U] == '\0' ||
                !af_project_image_hex(input[index + 1U], &high) ||
                !af_project_image_hex(input[index + 2U], &low)) {
                return false;
            }
            byte = (unsigned char)(high * 16U + low);
            index += 2U;
        }
        if (byte == 0U || byte < 32U || byte == 127U || write + 1U >= capacity) {
            return false;
        }
        output[write++] = (char)byte;
    }
    output[write] = '\0';
    return write != 0U;
}
