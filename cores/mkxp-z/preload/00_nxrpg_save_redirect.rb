# frozen_string_literal: false
# cores/mkxp-z/preload/00_nxrpg_save_redirect.rb
#
# Transparently isolates RGSS1/2/3 save files (`Save*.rxdata`, `Save*.rvdata`,
# `Save*.rvdata2`, `*.sav`, `save*.ini`, etc.) into `ENV['NXRPG_SAVE_DIR']`
# with atomic `<file>.tmp` + flush + fsync + rename and Copy-on-Write read
# fallback from the immutable game directory.

module NxRpgSaveRedirect
  SAVE_PATTERNS = [
    /\Asave\d*\.(?:rxdata|rvdata|rvdata2|lsd|sav|dat|bin|ini|json)\z/i,
    /\Aglobal\.(?:rxdata|rvdata|rvdata2)\z/i,
    /\.sav\z/i,
  ].freeze

  module_function

  def save_dir
    dir = ENV['NXRPG_SAVE_DIR']
    return nil if dir.nil? || dir.empty?

    dir.tr('\\', '/').sub(%r{/*\z}, '')
  end

  def normalize_rel(path)
    p = path.to_s.tr('\\', '/')
    return nil if p.empty?

    # Relative path or explicit path inside cwd (game folder)
    cwd = Dir.pwd.tr('\\', '/').sub(%r{/*\z}, '')
    if p.start_with?("#{cwd}/")
      p = p[(cwd.length + 1)..]
    elsif p.start_with?('./')
      p = p[2..]
    elsif p.start_with?('/') || p.include?(':')
      return nil
    end
    p
  end

  def save_relative_path?(rel)
    return false if rel.nil? || rel.empty?

    base = File.basename(rel)
    return true if rel.downcase.start_with?('save/', 'saves/')

    SAVE_PATTERNS.any? { |pat| pat.match?(base) }
  end

  def redirect_for_read(path)
    sdir = save_dir
    return path if sdir.nil?

    rel = normalize_rel(path)
    return path unless save_relative_path?(rel)

    isolated = "#{sdir}/#{rel}"
    return isolated if File.exist?(isolated)

    # Copy-on-Write fallback to pre-existing save in game folder
    path
  end

  def redirect_for_write(path)
    sdir = save_dir
    return path if sdir.nil?

    rel = normalize_rel(path)
    return path unless save_relative_path?(rel)

    isolated = "#{sdir}/#{rel}"
    ensure_dir(File.dirname(isolated))
    isolated
  end

  def ensure_dir(dir)
    return if dir.nil? || dir.empty? || File.directory?(dir)

    parent = File.dirname(dir)
    ensure_dir(parent) if parent != dir && !parent.empty?
    Dir.mkdir(dir) unless File.directory?(dir)
  rescue SystemCallError
    nil
  end

  def write_mode?(mode)
    return false if mode.nil?
    return (mode & (File::WRONLY | File::RDWR | File::CREAT | File::TRUNC | File::APPEND)) != 0 if mode.is_a?(Integer)

    m = mode.to_s
    m.include?('w') || m.include?('a') || m.include?('+')
  end

  def atomic_write_via_tmp(target_path, mode_str = 'wb', &block)
    tmp_path = "#{target_path}.tmp"
    ensure_dir(File.dirname(target_path))
    result = nil
    begin
      orig_open(tmp_path, mode_str) do |f|
        result = block.call(f)
        f.flush
        begin
          f.fsync
        rescue NotImplementedError, Errno::EINVAL
          nil
        end
      end
      File.rename(tmp_path, target_path)
    rescue Exception => e
      File.delete(tmp_path) if File.exist?(tmp_path)
      raise e
    end
    result
  end
end

if NxRpgSaveRedirect.save_dir
  NxRpgSaveRedirect.ensure_dir(NxRpgSaveRedirect.save_dir)

  class << File
    alias nxrpg_orig_open open
    alias nxrpg_orig_exist? exist?
    alias nxrpg_orig_file? file?
    alias nxrpg_orig_delete delete
    alias nxrpg_orig_unlink unlink
    alias nxrpg_orig_mtime mtime
    alias nxrpg_orig_size size

    def open(path, *args, &block)
      mode = args[0]
      if NxRpgSaveRedirect.write_mode?(mode)
        target = NxRpgSaveRedirect.redirect_for_write(path)
        if target != path && block_given? && !mode.is_a?(Integer) && mode.to_s.start_with?('w')
          tmp_path = "#{target}.tmp"
          NxRpgSaveRedirect.ensure_dir(File.dirname(target))
          res = nil
          begin
            res = nxrpg_orig_open(tmp_path, *args) do |f|
              r = block.call(f)
              f.flush
              begin
                f.fsync
              rescue NotImplementedError, Errno::EINVAL
                nil
              end
              r
            end
            File.rename(tmp_path, target)
          rescue Exception => e
            nxrpg_orig_delete(tmp_path) if nxrpg_orig_exist?(tmp_path)
            raise e
          end
          return res
        end
        return nxrpg_orig_open(target, *args, &block)
      else
        target = NxRpgSaveRedirect.redirect_for_read(path)
        return nxrpg_orig_open(target, *args, &block)
      end
    end

    def exist?(path)
      nxrpg_orig_exist?(NxRpgSaveRedirect.redirect_for_read(path))
    end

    def file?(path)
      nxrpg_orig_file?(NxRpgSaveRedirect.redirect_for_read(path))
    end

    def mtime(path)
      nxrpg_orig_mtime(NxRpgSaveRedirect.redirect_for_read(path))
    end

    def size(path)
      nxrpg_orig_size(NxRpgSaveRedirect.redirect_for_read(path))
    end

    def delete(*paths)
      mapped = paths.map { |p| NxRpgSaveRedirect.redirect_for_write(p) }
      existing = mapped.select { |p| nxrpg_orig_exist?(p) }
      return 0 if existing.empty?

      nxrpg_orig_delete(*existing)
    end

    alias unlink delete
  end

  class << Dir
    alias nxrpg_orig_glob glob

    def glob(pattern, *flags, &block)
      pats = pattern.is_a?(Array) ? pattern : [pattern]
      results = nxrpg_orig_glob(pats, *flags)
      sdir = NxRpgSaveRedirect.save_dir
      if sdir && File.directory?(sdir)
        pats.each do |pat|
          rel = NxRpgSaveRedirect.normalize_rel(pat)
          next unless rel

          if NxRpgSaveRedirect.save_relative_path?(rel) || rel.include?('*')
            extra = nxrpg_orig_glob("#{sdir}/#{rel}", *flags).map do |abs|
              abs.delete_prefix("#{sdir}/")
            end
            results.concat(extra)
          end
        end
        results.uniq!
      end
      if block_given?
        results.each(&block)
        return nil
      end
      results
    end
  end
end
