#! /usr/bin/env ruby

require "chipcard/gbp"
include Chipcard::GBP

require 'pp'

def prt(dir, p)
  print("#{dir} [")
  if p.length > 0
    for i in 0...p.length - 1
      printf(" 0x%02X,", p[i])
    end
    printf(" 0x%02X", p[-1])
  end
  print(" ]\n")
end

unless ARGV.length == 1
  puts "#{$0} <tty>"
  exit
end

p = Reader.new(ARGV[0])

cmd = [ 0x22, 0x05, 0x3F, 0xF0, 0x10 ]
prt '>', cmd
ans = p.send(cmd)
prt '<', ans

ans.shift

ans.collect! {|x| x.chr }
puts ans.to_s
