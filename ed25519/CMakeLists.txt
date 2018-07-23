cmake_minimum_required (VERSION 3.0)

project (ed25519)

file(GLOB SRC RELATIVE .
    src/*.h
    src/*.c
)

add_library(${PROJECT_NAME}
	       src/add_scalar.c
	       src/fe.c
	       src/ge.c
	       src/key_exchange.c
	       src/keypair.c
	       src/sc.c
	       src/seed.c
	       src/sha512.c
	       src/sign.c
	       src/verify.c
)

target_include_directories (${PROJECT_NAME} PUBLIC src)
