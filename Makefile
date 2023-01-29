huawei-sound-fix:
	@gcc huawei_matebook14s_codec_fix.cpp -o huawei-sound-fix
	@echo "Now run sudo make install"

.PHONY:
install: huawei-sound-fix
	@cp huawei-sound-fix /usr/local/bin/
	@chmod +x /usr/local/bin/huawei-sound-fix
	@cp *.service /etc/systemd/system/
	@systemctl daemon-reload
	@systemctl enable huawei-codec-fix.service
	@systemctl enable restart-huawei-codec-fix.service
	@systemctl start huawei-codec-fix.service
	@echo "Now reboot required"

.PHONY:
clear:
	@rm huawei-sound-fix