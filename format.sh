# https://www.cnblogs.com/__tudou__/p/13322854.html
# 对所有代码重新格式化，这个我改了一下，和原来的项目不一样
# 本项目不用提交，纯自己学习，缩进注释等格式按个人风格来
find . -regex '.*\.\(cpp\|hpp\|cu\|c\|h\)' ! -regex '.*\(pb\.h\|pb\.cc\)$' -exec clang-format -style=file -i {} \;