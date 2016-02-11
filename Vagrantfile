# -*- mode: ruby -*-
# vi: set ft=ruby :

disk = '.vagrant/machines/default/virtualbox/sdb.vdi'

Vagrant.configure(2) do |config|
  config.vm.box = "ubuntu/trusty64"

  config.vm.provider "virtualbox" do |vb|
    unless File.exist?(disk)
      # Create disk with 4 MB size
      vb.customize ['createhd', '--filename', disk, '--variant', 'Fixed', '--size', 4]
      vb.customize ['storageattach', :id, '--storagectl', 'SATAController', '--port', 1, '--device', 0, '--type', 'hdd', '--medium', disk]
    end
  end

  config.vm.provision "shell", path: "provision.sh"
end
