# frozen_string_literal: false
# cores/mkxp-z/preload/01_nxrpg_win32_ext.rb
#
# Loaded AFTER upstream `scripts/preload/win32_wrap.rb` when
# `manifest.coreOptions.mkxp.enableWin32ApiStubs == true`.
#
# Extends upstream `Win32API` (`User32` input/mouse/window stubs) with commonly
# used `Kernel32` INI helpers, `User32::GetSystemMetrics`, and `MessageBoxA/W`
# logging so Japanese/Chinese RGSS1/2/3 doujin games run without crashes.

module NxRpgWin32Ext
  module_function

  def read_ini_value(file_path, section, key, default_val)
    target = defined?(NxRpgSaveRedirect) ? NxRpgSaveRedirect.redirect_for_read(file_path) : file_path
    return default_val unless File.exist?(target)

    cur_sec = nil
    sec_down = section.to_s.downcase
    key_down = key.to_s.downcase
    File.foreach(target, mode: 'rb') do |raw_line|
      line = raw_line.force_encoding('UTF-8').scrub('').strip
      next if line.empty? || line.start_with?(';', '#')

      if line.start_with?('[') && line.end_with?(']')
        cur_sec = line[1..-2].strip.downcase
        next
      end
      next unless cur_sec == sec_down

      eq = line.index('=')
      next unless eq

      k = line[0...eq].strip.downcase
      return line[(eq + 1)..].strip if k == key_down
    end
    default_val
  rescue StandardError
    default_val
  end

  def write_ini_value(file_path, section, key, value)
    target = defined?(NxRpgSaveRedirect) ? NxRpgSaveRedirect.redirect_for_write(file_path) : file_path
    lines = if File.exist?(target)
              File.read(target, mode: 'rb').force_encoding('UTF-8').scrub('').lines.map(&:chomp)
            else
              []
            end

    sec_header = "[#{section}]"
    sec_down = section.to_s.downcase
    key_down = key.to_s.downcase
    in_sec = false
    sec_end_idx = nil
    updated = false

    lines.each_with_index do |line, idx|
      stripped = line.strip
      if stripped.start_with?('[') && stripped.end_with?(']')
        if in_sec && !updated
          sec_end_idx = idx
          break
        end
        in_sec = (stripped[1..-2].strip.downcase == sec_down)
      elsif in_sec
        eq = stripped.index('=')
        if eq && stripped[0...eq].strip.downcase == key_down
          lines[idx] = "#{key}=#{value}"
          updated = true
          break
        end
      end
    end

    unless updated
      if in_sec
        insert_at = sec_end_idx || lines.length
        lines.insert(insert_at, "#{key}=#{value}")
      else
        lines << sec_header
        lines << "#{key}=#{value}"
      end
    end

    payload = lines.join("\r\n") + "\r\n"
    if defined?(NxRpgSaveRedirect)
      NxRpgSaveRedirect.ensure_dir(File.dirname(target))
      tmp = "#{target}.tmp"
      File.binwrite(tmp, payload)
      File.rename(tmp, target)
    else
      File.binwrite(target, payload)
    end
    1
  rescue StandardError
    0
  end
end

if defined?(Win32API)
  class Win32API
    alias nxrpg_orig_initialize initialize
    alias nxrpg_orig_call call

    def initialize(dll, func, import = 'V', export = 'V', calltype = :stdcall)
      @nxrpg_dll = dll.to_s.downcase.sub(/\.dll\z/, '')
      @nxrpg_func = func.to_s
      nxrpg_orig_initialize(dll, func, import, export, calltype)
    end

    def call(*args)
      case [@nxrpg_dll, @nxrpg_func]
      when ['kernel32', 'GetPrivateProfileStringA'], ['kernel32', 'GetPrivateProfileString']
        sec, key, default_val, buf, buf_size, filename = args
        val = NxRpgWin32Ext.read_ini_value(filename.to_s, sec.to_s, key.to_s, default_val.to_s)
        out = val.byteslice(0, [buf_size.to_i - 1, 0].max) || ''
        if buf.is_a?(String)
          buf.replace(out.ljust(buf.bytesize, "\0"))
        end
        return out.bytesize
      when ['kernel32', 'GetPrivateProfileIntA'], ['kernel32', 'GetPrivateProfileInt']
        sec, key, default_int, filename = args
        val = NxRpgWin32Ext.read_ini_value(filename.to_s, sec.to_s, key.to_s, default_int.to_s)
        return val.to_i
      when ['kernel32', 'WritePrivateProfileStringA'], ['kernel32', 'WritePrivateProfileString']
        sec, key, val, filename = args
        return NxRpgWin32Ext.write_ini_value(filename.to_s, sec.to_s, key.to_s, val.to_s)
      when ['user32', 'GetSystemMetrics']
        idx = args[0].to_i
        return 1280 if idx == 0 # SM_CXSCREEN
        return 720 if idx == 1  # SM_CYSCREEN
        return 0
      when ['user32', 'MessageBoxA'], ['user32', 'MessageBoxW']
        return 1 # IDOK
      end

      nxrpg_orig_call(*args)
    end
  end
end
