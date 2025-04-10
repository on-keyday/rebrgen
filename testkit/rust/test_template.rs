mod save;
use std::fs::File;
use std::io::Read;
use std::io::Write;

use std::env;

fn main() -> Result<(), save::Error> {
    let mut args = env::args();
    // first argument is path to input file
    let input_file = args.nth(1).expect("No input file provided");
    // second argument is path to output file
    let output_file = args.next().expect("No output file provided");
    // read input file as bytes
    let input = File::open(input_file).expect("Failed to open input file");
    let mut input_file = std::io::BufReader::new(input);
    let mut input = Vec::new();
    input_file
        .read_to_end(&mut input)
        .expect("Failed to read input file");
    let decoded = match save::TEST_CLASS::decode_exact(&mut input) {
        Ok(decoded) => decoded,
        Err(e) => {
            eprintln!("Failed to decode input file: {}", e);
            return Err(e); // gracefully, this returns 1 to the caller
        }
    };
    // write decoded data to output file
    let mut output = File::create(output_file).expect("Failed to create output file");
    let mut output_file = std::io::BufWriter::new(output);
    decoded
        .encode(&mut output_file)
        .expect("Failed to encode output file");
    output_file.flush().expect("Failed to flush output file");
    Ok(())
}
