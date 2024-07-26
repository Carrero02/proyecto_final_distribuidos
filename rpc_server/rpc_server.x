struct operation {
    string username<256>;
    string operation<269>;  /* 12 Bytes for operation name, 255 for filename (in case of PUBLISH or DELETE), 1 for space, 1 for null terminator */
    string timestamp<20>;
};

program OPERATIONLOG {
    version OPERATIONLOG_V1 {
        void LOG_OPERATION(operation) = 1;
    } = 1;
} = 0x20000099;