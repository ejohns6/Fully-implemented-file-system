all: vmufs

vmufs: vmufs.c
	gcc -Wall vmufs.c `pkg-config fuse --cflags --libs` -o vmufs

.PHONY: clean test1 test2 test3 test4 unmnt restore

test1: vmufs
	./vmufs blank.img mnt
	ls -l mnt

test2: vmufs
	./vmufs populated1.img mnt
	ls -l mnt
	cat mnt/BERSERK_DATA
	
	cat mnt/CRAZYTAXI_DC

	cat mnt/SHENMUE2_001


test3: vmufs
	./vmufs populated2.img mnt
	ls -l mnt
	cat mnt/CHU_CHU__RCT
	cat mnt/C_TAXI02.SYS
	cat mnt/ECCODOTF.___
	cat mnt/ECCODOTF.RLB
	cat mnt/GAUNTLET.001
	cat mnt/KAO_KANG.000
	cat mnt/KAO_KANG.001
	cat mnt/MDK2SAVE.SAV
	cat mnt/RAYMAN_2.RLB
	cat mnt/RAYMAN_2.SY_
	cat mnt/RE_CV000.001
	cat mnt/SAMBAUS1.SYS
	cat mnt/SONIC2___ALF
	cat mnt/SONIC2___S01
	cat mnt/SONICADV_ALF
	cat mnt/SONICADV_INT
	cat mnt/SPMAGNEO.DT0


test4: vmufs
	mkdir -p mnt
	./vmufs populated3.img mnt
	ls -l mnt

unmnt:
	fusermount -u mnt

restore:
	cp populated2-vanilla.img populated2.img

clean:
	-rm vmufs
