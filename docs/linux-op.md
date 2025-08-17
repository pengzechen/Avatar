

cat /proc/devices
mknod /dev/vda b 254 0
mount -t vfat /dev/vda /mnt -o uid=0,gid=0
umount /mnt


## 创建一个空的 64M fat32 文件系统
dd if=/dev/zero of=test.img bs=1M count=64
mkfs.vfat -F 32 test.img
dd if=/dev/zero of=test2.img bs=1M count=64
mkfs.vfat -F 32 test2.img

## 在宿主机挂载
sudo mkdir -p /mnt/testimg
sudo mount -o loop test.img /mnt/testimg
sudo umount /mnt/testimg