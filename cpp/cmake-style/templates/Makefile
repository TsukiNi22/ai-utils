#**************************************************************#
#Edition:
#**  <DD/MM/YYYY> by Tsukini
#
#File Name:
#**  Makefile
#
#File Description:
#**  Call the CMake
#**************************************************************#

BUILD_DIR := build

all:
	cmake -S . -B $(BUILD_DIR)
	cmake --build $(BUILD_DIR) --parallel $$(nproc)

clean:
	-cmake --build $(BUILD_DIR) --target clean 2> /dev/null

fclean: clean
	-rm -rf $(BUILD_DIR) 2> /dev/null

.NOTPARALLEL:
re: fclean all

.PHONY: all clean fclean re
