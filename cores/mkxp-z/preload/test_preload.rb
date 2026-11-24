# frozen_string_literal: true

require 'tmpdir'
require 'fileutils'

# Minimal mock of upstream Win32API for host testing 01_nxrpg_win32_ext.rb
class Win32API
  def initialize(*_args); end
  def call(*_args); 0; end
end

Dir.mktmpdir('nxrpg_mkxp_preload_') do |tmp|
  game_dir = File.join(tmp, 'game')
  save_dir = File.join(tmp, 'save_isolated')
  FileUtils.mkdir_p(game_dir)
  File.binwrite(File.join(game_dir, 'Save01.rvdata2'), 'LEGACY_SAVE_BYTES')

  ENV['NXRPG_SAVE_DIR'] = save_dir
  Dir.chdir(game_dir) do
    load File.expand_path('00_nxrpg_save_redirect.rb', __dir__)
    load File.expand_path('01_nxrpg_win32_ext.rb', __dir__)

    # 1. Copy-on-Write read fallback from game_dir
    raise 'Expected CoW Save01.rvdata2 to exist' unless File.exist?('Save01.rvdata2')
    raise 'Unexpected CoW content' unless File.binread('Save01.rvdata2') == 'LEGACY_SAVE_BYTES'

    # 2. Writes go strictly to save_dir without touching game_dir
    File.open('Save01.rvdata2', 'wb') { |f| f.write('NEW_ISOLATED_SAVE') }
    raise 'Game dir save should stay untouched' unless File.binread(File.join(game_dir, 'Save01.rvdata2')) == 'LEGACY_SAVE_BYTES'
    raise 'Isolated save missing' unless File.binread(File.join(save_dir, 'Save01.rvdata2')) == 'NEW_ISOLATED_SAVE'
    raise 'Leftover .tmp found' if File.exist?(File.join(save_dir, 'Save01.rvdata2.tmp'))

    # 3. Dir.glob merges isolated saves
    File.open('Save02.rvdata2', 'wb') { |f| f.write('SLOT2') }
    globbed = Dir.glob('Save*.rvdata2').sort
    raise "Unexpected Dir.glob result: #{globbed.inspect}" unless globbed == ['Save01.rvdata2', 'Save02.rvdata2']

    # 4. Kernel32 INI read/write redirects to save_dir
    w_ini = Win32API.new('kernel32', 'WritePrivateProfileStringA', 'PPPP', 'I')
    r_ini = Win32API.new('kernel32', 'GetPrivateProfileStringA', 'PPPPIP', 'I')
    r_int = Win32API.new('kernel32', 'GetPrivateProfileIntA', 'PPPI', 'I')
    sys_m = Win32API.new('user32', 'GetSystemMetrics', 'I', 'I')

    raise 'WritePrivateProfileStringA failed' unless w_ini.call('Config', 'Difficulty', '2', 'save.ini') == 1
    raise 'INI leaked into game_dir' if File.nxrpg_orig_exist?(File.join(game_dir, 'save.ini'))
    raise 'INI missing in save_dir' unless File.nxrpg_orig_exist?(File.join(save_dir, 'save.ini'))

    buf = "\0" * 64
    len = r_ini.call('Config', 'Difficulty', '0', buf, 64, 'save.ini')
    raise "Expected len=1, got #{len}" unless len == 1
    raise "Expected '2', got #{buf[0, len]}" unless buf[0, len] == '2'
    raise 'GetPrivateProfileIntA failed' unless r_int.call('Config', 'Difficulty', 0, 'save.ini') == 2
    raise 'GetSystemMetrics failed' unless sys_m.call(0) == 1280 && sys_m.call(1) == 720
  end
end

puts 'All mkxp-z Ruby preload checks passed.'
