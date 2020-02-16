# Distributions that don't use syslog-ng should do something 
# different here, but note that the log file path in asunder.conf
# is also mentioned in the Preferences UI.
#
install:
	@cp -v asunder.conf /etc/rsyslog.d/ || echo "It appears that your system is not using syslog-ng. That's ok but Asunder will be logging to the default syslog target instead of /var/log/asunder.log"

uninstall:
	rm -f /etc/rsyslog.d/asunder.conf
